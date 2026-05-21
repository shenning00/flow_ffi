import 'package:test/test.dart';
import 'package:flow_ffi/src/models/environment.dart';
import 'package:flow_ffi/src/models/graph.dart';
import 'package:flow_ffi/src/models/factory.dart';
import 'package:flow_ffi/src/models/node.dart';
import 'package:flow_ffi/src/utils/error_handler.dart';

/// Tests for [Environment.postSetInputData] — the Dart binding for the
/// C symbol `flow_env_add_task_set_input_data` (FLOW_RUN.html §10.11, P1).
///
/// The wrapper posts a `SetInputData(portKey, data, compute=true)` task to
/// the env's worker pool. It is fire-and-forget: the call returns
/// synchronously on the calling isolate; the actual write + downstream
/// Compute() cascade runs on a worker thread, and `env.wait()` is the
/// barrier that proves the worker drained.
///
/// On the C++ side the lambda captures `shared_ptr` copies of the node and
/// data, so it is safe for the Dart side to release the FFI data handle
/// immediately after the symbol returns — backing native memory lives
/// until the worker is done.
void main() {
  group('Environment.postSetInputData — input validation', () {
    setUp(() {
      ErrorHandler.clearError();
    });

    test('empty portKey throws InvalidArgumentException', () {
      // Construct a real Environment so the wrapper has a valid env to test
      // the early-return validation against. We never reach the FFI call
      // because the empty-portKey check fires first — so we don't need a
      // real Node either; a never-touched dynamic stand-in is fine.
      final env = Environment(maxThreads: 1);
      try {
        // We pass a deliberately-invalid Node placeholder via `as Node` —
        // it's never dereferenced because the empty-key check throws first.
        // To keep the test fully type-safe and self-contained, route through
        // the Graph + try-to-create-a-node path; if no nodes are loadable
        // we can still construct a Graph and the wrapper still throws.
        final graph = Graph(env);
        try {
          final factory = env.factory;
          try {
            Node? probe;
            final categories = factory.getCategories();
            for (final category in categories) {
              final classes = factory.getNodeClasses(category);
              if (classes.isNotEmpty) {
                try {
                  probe = graph.addNode(classes.first, 'probe');
                  break;
                } catch (_) {
                  continue;
                }
              }
            }

            if (probe == null) {
              // No registered node classes — the wrapper still has to throw
              // on an empty key before it dereferences anything. To exercise
              // that we'd need *some* Node; if we can't get one, document
              // and skip rather than fake a Node. The contract is identical
              // to Node.setInputData (line 233-236 of node.dart) so the
              // input-validation behaviour is structurally covered by that
              // file's existing test surface.
              print('[postSetInputData] No registered node classes available '
                  '— skipping empty-portKey assertion (probe Node unavailable).');
              return;
            }

            expect(
              () => env.postSetInputData(probe!, '', 42),
              throwsA(isA<InvalidArgumentException>()),
            );
          } finally {
            factory.dispose();
          }
        } finally {
          graph.dispose();
        }
      } finally {
        env.dispose();
      }
    });
  });

  group('Environment.postSetInputData — happy path & lifetime', () {
    late Environment env;
    late Graph graph;
    late NodeFactory factory;

    setUp(() {
      ErrorHandler.clearError();
      env = Environment(maxThreads: 2);
      graph = Graph(env);
      factory = env.factory;
    });

    tearDown(() {
      graph.dispose();
      factory.dispose();
      env.dispose();
    });

    /// Picks any node class with at least one input port, or null if no
    /// module is loaded. This mirrors the discovery pattern in
    /// `graph_execution_test.dart`.
    Node? _findNodeWithInputPort() {
      final categories = factory.getCategories();
      for (final category in categories) {
        final classes = factory.getNodeClasses(category);
        for (final className in classes) {
          try {
            final node = graph.addNode(className, 'probe_$className');
            if (node.getInputPortKeys().isNotEmpty) {
              return node;
            }
            graph.removeNode(node.id);
          } catch (_) {
            // Skip nodes that fail to create
            continue;
          }
        }
      }
      return null;
    }

    test('happy path: post → wait → read-back round-trips', () {
      final node = _findNodeWithInputPort();
      if (node == null) {
        print('[postSetInputData] No node module with input ports loaded — '
            'skipping happy-path integration test.');
        return;
      }

      final portKey = node.getInputPortKeys().first;

      // Fire-and-forget: returns synchronously on the calling isolate.
      expect(
        () => env.postSetInputData(node, portKey, 42),
        returnsNormally,
        reason: 'postSetInputData should return immediately without throwing',
      );

      // Barrier: wait for the worker to drain the queued task.
      env.wait();

      // The worker invoked SetInputData on the node — reading the port
      // back must yield the value we posted. If the C++ symbol or its
      // binding name is wrong, this fails.
      final readBack = node.getInputData<int>(portKey);
      expect(readBack, equals(42),
          reason: 'Posted value should be visible after env.wait()');
    });

    test('lifetime: Dart-side handle released immediately, worker still sees data',
        () {
      final node = _findNodeWithInputPort();
      if (node == null) {
        print('[postSetInputData] No node module with input ports loaded — '
            'skipping lifetime test.');
        return;
      }

      final portKey = node.getInputPortKeys().first;

      // Construct the value in a tight scope and post it. The Dart wrapper
      // releases the FFI data handle in its `finally` block before the
      // worker has run. The C++ lambda's shared_ptr copy is what keeps
      // the backing storage alive. If that contract were broken, the
      // worker's write would touch freed memory and the read-back below
      // would either crash or yield garbage.
      void postScoped() {
        // Construct an int literal — no Dart-side reference to anything
        // outlives this function.
        env.postSetInputData(node, portKey, 7777);
      }

      postScoped();

      // No Dart-side reference to the value, the converter, or the FFI
      // handle survives past `postScoped()`. The worker still has to be
      // able to complete the write.
      env.wait();

      final readBack = node.getInputData<int>(portKey);
      expect(readBack, equals(7777),
          reason: 'Worker must complete the write even though the Dart-side '
              'FFI data handle has already been disposed');
    });
  });
}
