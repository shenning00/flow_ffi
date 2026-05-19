// F1 regression test — borrowed-handle ownership in EventManager.
//
// Scenario (per `/tmp/f1_dart_spec.md` §4):
//   1. Create an Environment + Graph.
//   2. Add a node via the factory (test_module fixture if available).
//   3. Register `onCompute` — the BORROWED-handle path
//      (`event_bridge.cpp:311`). Trigger an event, capture and then drop
//      every Dart reference to the event-delivered Node.
//   4. Force a Dart VM GC via `vm_service.getAllocationProfile(gc: true)`
//      and drain finalizer microtasks.
//   5. Assert that the ORIGINAL mapped node's handle remains `isValid`
//      and that `setInputData` (or another node op) still succeeds. Before
//      F1, the borrowed wrapper's GC finalizer would release the shared
//      underlying object — making this fail with
//      "node is not registered" / `InvalidHandleException`.
//
// SELF-SKIP: this test depends on (a) the native libflow_ffi dylib being
// loadable, and (b) at least one node class being registered in the factory
// (i.e. a module fixture). When either is absent it skips cleanly — the
// dylib path resolution is unchanged from the rest of the package's tests
// (`lib/src/ffi/bindings.dart`) and may differ between developer machines.

import 'dart:async';
import 'dart:developer';
import 'dart:io';
import 'dart:isolate' as dart_isolate;

import 'package:test/test.dart';
// ignore: depend_on_referenced_packages
import 'package:vm_service/vm_service_io.dart';

import 'package:flow_ffi/src/models/environment.dart';
import 'package:flow_ffi/src/models/graph.dart';
import 'package:flow_ffi/src/models/node.dart';
import 'package:flow_ffi/src/models/module.dart';

/// Force a full GC via the VM service (deterministic) and drain pending
/// finalizer microtasks. Falls back to allocation pressure when the VM
/// service URI is unavailable (e.g. the test process was started without
/// `--enable-vm-service`).
Future<void> _forceGc() async {
  final info = await Service.getInfo();
  final uri = info.serverUri;
  if (uri == null) {
    // Fallback: allocation pressure. Less deterministic; interleaved with
    // event-loop yields so the scavenger has a chance to run.
    for (var i = 0; i < 200; i++) {
      final filler = List<int>.filled(1 << 16, i);
      // Touch the buffer so it cannot be optimised away.
      filler[0] = i;
      if (i % 16 == 0) await Future<void>.delayed(Duration.zero);
    }
    return;
  }
  final wsUri = uri.replace(
    scheme: uri.scheme == 'https' ? 'wss' : 'ws',
    path: '${uri.path}ws',
  );
  final vm = await vmServiceConnectUri(wsUri.toString());
  try {
    // Service.getIsolateId is technically since SDK 3.2.0, but the package
    // SDK constraint is `>=3.1.0`. This is test-only code that self-skips
    // when the VM service cannot be reached; the version-since warning is
    // suppressed because the wider package does not depend on this API.
    // ignore: sdk_version_since
    final isolateId = Service.getIsolateId(dart_isolate.Isolate.current);
    if (isolateId != null) {
      await vm.getAllocationProfile(isolateId, gc: true);
    }
  } finally {
    await vm.dispose();
  }
}

/// Two rounds of GC + microtask drain so Finalizer callbacks (which run on
/// later turns) have a chance to fire before the assertion.
Future<void> _gcAndDrain() async {
  for (var round = 0; round < 3; round++) {
    await _forceGc();
    await Future<void>.delayed(Duration.zero);
    await Future<void>.delayed(const Duration(milliseconds: 1));
  }
}

void main() {
  // Resolve the test_module fixture (built by
  // `examples/fl_nodes_example/test/fixtures/test_module`). It is the only
  // in-tree module that exposes a node class with at least one input port,
  // which is needed for the final `setInputData` assertion.
  final candidatePaths = <String>[
    [
      Directory.current.path,
      '..',
      '..',
      'flutter_fl_nodes',
      'examples',
      'fl_nodes_example',
      'test',
      'fixtures',
      'test_module',
      'build',
      'test_module.fmod',
    ].join(Platform.pathSeparator),
  ];
  final fmodPath = candidatePaths.firstWhere(
    (p) => File(p).existsSync(),
    orElse: () => '',
  );

  group('F1 — borrowed event Node does not free the mapped node', () {
    test('after GC, the mapped node remains valid and usable', () async {
      // Step 0: load the native library. Any FFI lookup will throw if the
      // dylib resolution fails; the test self-skips in that case.
      Environment env;
      try {
        env = Environment(maxThreads: 2);
      } catch (e) {
        markTestSkipped(
          'Could not load libflow_ffi (dlopen path issue is a known '
          'pre-existing condition for package-level tests): $e',
        );
        return;
      }

      Graph? graph;
      Module? module;
      try {
        graph = Graph(env);
        final factory = env.factory;

        if (fmodPath.isEmpty) {
          markTestSkipped(
            'test_module.fmod not found at any candidate path. Build the '
            'fixture in '
            'flutter_fl_nodes/examples/fl_nodes_example/test/fixtures/'
            'test_module before running this test.',
          );
          return;
        }

        module = Module(factory);
        final loaded = await module.load(fmodPath);
        if (!loaded || !module.isLoaded) {
          markTestSkipped(
            'Failed to load test_module.fmod from $fmodPath',
          );
          return;
        }
        module.registerNodes();

        // Pick any node class with an input port — required for the
        // `setInputData` assertion at the end.
        final categories = factory.getCategories();
        String? nodeClass;
        for (final cat in categories) {
          for (final cls in factory.getNodeClasses(cat)) {
            try {
              final probe = graph.addNode(cls, 'probe');
              final inputs = probe.getInputPortKeys();
              graph.removeNode(probe.id);
              if (inputs.isNotEmpty) {
                nodeClass = cls;
                break;
              }
            } catch (_) {
              // skip unconstructable classes
            }
          }
          if (nodeClass != null) break;
        }
        if (nodeClass == null) {
          markTestSkipped(
            'No node class with an input port was registered from '
            'test_module.fmod',
          );
          return;
        }

        // Step 1: create the OWNING mapped node — mirrors the
        // _nodeMapping[id] entry held by FlowBridge.
        final Node mapped = graph.addNode(nodeClass, 'mapped_n1');
        final inputPort = mapped.getInputPortKeys().first;

        // Step 2: register onCompute and capture the borrowed event Node
        // delivered to the stream. Capturing it inside an immediately-
        // exited helper keeps the reference local so it can be cleared
        // without bleeding into the enclosing scope.
        Node? deliveredEventNode;
        final sub = mapped.onCompute.listen((evt) {
          deliveredEventNode = evt.node;
        });

        // Trigger compute. flow_node_on_compute fires when the node is
        // executed; running the graph (with the source set up) drives the
        // dispatcher. For nodes that need explicit invocation we fall
        // through to compute() if graph.run() did not fire the event.
        try {
          graph.run();
          env.wait();
        } catch (_) {
          // Some node classes may not be runnable standalone — ignore.
        }
        if (deliveredEventNode == null) {
          try {
            mapped.compute();
          } catch (_) {/* tolerate */}
        }
        // Allow the NativeCallable.listener delivery turn to land.
        await Future<void>.delayed(const Duration(milliseconds: 20));
        await Future<void>.delayed(Duration.zero);

        // If we still did not receive an event, this node class does not
        // emit onCompute under the harness's conditions — skip rather
        // than mis-report a F1 regression that wasn't exercised.
        if (deliveredEventNode == null) {
          await sub.cancel();
          markTestSkipped(
            'onCompute event was not delivered by node class "$nodeClass"; '
            'the BORROWED-handle code path could not be exercised.',
          );
          return;
        }

        // Step 3: drop every Dart reference to the borrowed wrapper.
        deliveredEventNode = null;

        // Step 4: force GC + finalizer drain.
        await _gcAndDrain();

        // Step 5: assert the original mapped node is still alive and
        // usable. Before F1 this would fail because the borrowed
        // wrapper's finalizer released the shared underlying object.
        expect(
          mapped.isValid,
          isTrue,
          reason:
              'Mapped node must remain valid after the borrowed event '
              'Node is GC\'d (F1 regression — the event wrapper must not '
              'attach a finalizer to a borrowed handle).',
        );

        // setInputData exercises flow_node_set_input_data which fails
        // with "node is not registered" if the underlying object has
        // been released.
        expect(
          () => mapped.setInputData(inputPort, 1),
          returnsNormally,
          reason:
              'setInputData on the mapped node must still succeed after '
              'a GC cycle that collected the borrowed event wrapper.',
        );

        await sub.cancel();
      } finally {
        try {
          if (module != null) {
            if (module.isLoaded) module.unregisterNodes();
            module.dispose();
          }
          graph?.dispose();
          env.dispose();
        } catch (_) {
          // Cleanup errors are non-fatal for the test outcome.
        }
      }
    }, timeout: const Timeout(Duration(seconds: 30)));
  });
}
