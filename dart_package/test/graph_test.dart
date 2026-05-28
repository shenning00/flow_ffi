import 'dart:io';

import 'package:test/test.dart';
import 'package:flow_ffi/flow_ffi.dart';

/// Resolve the `test_module.fmod` fixture, which registers the `TestNode`
/// class used by the addNode tests below. Mirrors the candidate-path logic
/// from `event_handle_ownership_test.dart` and the user-documented flow-core
/// build directory.
String _resolveTestModuleFmod() {
  final candidates = <String>[
    [
      Directory.current.path,
      '..',
      '..',
      'flow-core',
      'build_test',
      'tests',
      'test_module.fmod',
    ].join(Platform.pathSeparator),
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
  return candidates.firstWhere(
    (p) => File(p).existsSync(),
    orElse: () => '',
  );
}

void main() {
  group('Graph Tests', () {
    test('should create and destroy graph', () {
      try {
        // Create environment
        final env = Environment(maxThreads: 4);
        expect(env.isValid, isTrue);

        // Create graph
        final graph = Graph(env);
        expect(graph.isValid, isTrue);

        print('Graph created successfully');

        // Clean up
        graph.dispose();
        env.dispose();
      } catch (e) {
        print('Error: $e');
        fail('Graph creation failed: $e');
      }
    });

    test('should handle graph operations', () {
      try {
        // Create environment
        final env = Environment(maxThreads: 4);
        final graph = Graph(env);

        // Try to get all nodes (should be empty)
        final nodes = graph.getNodes();
        expect(nodes, isEmpty);

        // Try to run empty graph (should work)
        graph.run();

        // Try to save to JSON
        final jsonStr = graph.saveToJson();
        expect(jsonStr, isNotNull);
        expect(jsonStr.length, greaterThan(0));

        // Print JSON (truncate if too long)
        final preview =
            jsonStr.length > 100 ? '${jsonStr.substring(0, 100)}...' : jsonStr;
        print('Graph JSON: $preview');

        // Clean up
        graph.dispose();
        env.dispose();
      } catch (e) {
        print('Error in graph operations: $e');
        fail('Graph operations failed: $e');
      }
    });
  });

  // ---------------------------------------------------------------------------
  // ADD_NODE_FROM_JSON Phase 2 — addNode(uuid:) tests.
  //
  // These tests exercise the new optional UUID parameter on Graph.addNode
  // (which routes through the new `flow_graph_add_node_with_uuid` FFI symbol).
  // They require a loaded fmod that registers at least one constructible
  // node class — we use `test_module.fmod` (class `TestNode`).
  //
  // NOTE on signature: the design doc proposed a "mixed positional+named"
  // shape `Node addNode(String classId, [String? name], {String? uuid})`,
  // which Dart's grammar does not allow (optional positional and named
  // parameters cannot coexist in a single declaration). The signature
  // actually used is `Node addNode(String classId, [String? name, String? uuid])`
  // — both optional positional. This preserves every existing positional
  // caller bit-for-bit; new callers wanting an explicit UUID pass it as the
  // third positional argument. Tests below use the positional form.
  // ---------------------------------------------------------------------------
  group('Graph.addNode UUID parameter (ADD_NODE_FROM_JSON Phase 2)', () {
    late String fmodPath;
    setUpAll(() {
      fmodPath = _resolveTestModuleFmod();
    });

    /// Build (env, graph, module-with-TestNode-registered) and return the
    /// node class name to use. Returns null when the test module cannot be
    /// loaded, allowing the test to self-skip in that case.
    Future<({Environment env, Graph graph, Module module, String className})?>
        _setup() async {
      if (fmodPath.isEmpty) return null;
      final env = Environment(maxThreads: 2);
      final graph = Graph(env);
      final factory = env.factory;
      final module = Module(factory);
      final loaded = await module.load(fmodPath);
      if (!loaded || !module.isLoaded) {
        graph.dispose();
        env.dispose();
        return null;
      }
      module.registerNodes();

      // Find a constructible class.
      String? className;
      for (final cat in factory.getCategories()) {
        for (final cls in factory.getNodeClasses(cat)) {
          try {
            final probe = graph.addNode(cls, 'probe');
            graph.removeNode(probe.id);
            className = cls;
            break;
          } catch (_) {
            // skip unconstructable classes
          }
        }
        if (className != null) break;
      }
      if (className == null) {
        if (module.isLoaded) module.unregisterNodes();
        module.dispose();
        graph.dispose();
        env.dispose();
        return null;
      }
      return (env: env, graph: graph, module: module, className: className);
    }

    void _teardown(
      ({Environment env, Graph graph, Module module, String className}) ctx,
    ) {
      try {
        if (ctx.module.isLoaded) ctx.module.unregisterNodes();
        ctx.module.dispose();
      } catch (_) {}
      ctx.graph.dispose();
      ctx.env.dispose();
    }

    test('explicit UUID round-trip — node.id equals the requested UUID',
        () async {
      final ctx = await _setup();
      if (ctx == null) {
        markTestSkipped('test_module.fmod not available — skipping');
        return;
      }
      try {
        const wanted = '11111111-1111-1111-1111-111111111111';
        final node = ctx.graph.addNode(ctx.className, 'my-name', wanted);
        expect(node.id, equals(wanted));
      } finally {
        _teardown(ctx);
      }
    });

    test('no-UUID equivalence — old positional signature still works and '
        'produces a fresh, non-zero UUID', () async {
      final ctx = await _setup();
      if (ctx == null) {
        markTestSkipped('test_module.fmod not available — skipping');
        return;
      }
      try {
        // Old positional form — same call style as
        // example/simple_workflow.dart:51 and friends. This must continue
        // to compile and work after Phase 2.
        final node = ctx.graph.addNode(ctx.className, 'my-name');
        expect(node.name, equals('my-name'));
        // A real v4 UUID from the engine — not the zero UUID, not empty.
        expect(node.id, isNot(equals('00000000-0000-0000-0000-000000000000')));
        expect(node.id, isNotEmpty);
        // Sanity-check the shape (8-4-4-4-12 hex).
        expect(
          node.id,
          matches(RegExp(
              r'^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-'
              r'[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')),
        );
      } finally {
        _teardown(ctx);
      }
    });

    test('explicit UUID + name together — both stick on the node', () async {
      final ctx = await _setup();
      if (ctx == null) {
        markTestSkipped('test_module.fmod not available — skipping');
        return;
      }
      try {
        const wanted = '22222222-2222-2222-2222-222222222222';
        final node = ctx.graph.addNode(ctx.className, 'my-name', wanted);
        expect(node.id, equals(wanted));
        expect(node.name, equals('my-name'));
      } finally {
        _teardown(ctx);
      }
    });

    test('malformed UUID surfaces InvalidArgumentException', () async {
      final ctx = await _setup();
      if (ctx == null) {
        markTestSkipped('test_module.fmod not available — skipping');
        return;
      }
      try {
        // Per Phase 0, flow-core's UUID(string) throws std::invalid_argument
        // on malformed input; the FFI catches it, sets
        // FLOW_ERROR_INVALID_ARGUMENT, and returns nullptr. The Dart
        // wrapper's ErrorHandler.checkError() then throws
        // InvalidArgumentException — matching the rest of the FFI's
        // error-handling convention (see e.g. factory.dart createNode +
        // factory_test.dart's empty-class-name test).
        expect(
          () => ctx.graph.addNode(ctx.className, 'my-name', 'not-a-uuid'),
          throwsA(isA<InvalidArgumentException>()),
        );
      } finally {
        _teardown(ctx);
      }
    });

    test('back-compat — single-name positional call works (no uuid arg)',
        () async {
      // Distinct from the "no-UUID equivalence" test above: this one
      // exercises the documented positional call site shape from
      // example/simple_workflow.dart:51 and asserts the node is created
      // with the supplied name. Serves as a pinning test that Phase 2's
      // additive signature change did not regress the legacy two-arg call.
      final ctx = await _setup();
      if (ctx == null) {
        markTestSkipped('test_module.fmod not available — skipping');
        return;
      }
      try {
        final node = ctx.graph.addNode(ctx.className, 'name-only');
        expect(node.name, equals('name-only'));
        expect(node.id, isNotEmpty);
      } finally {
        _teardown(ctx);
      }
    });
  });
}
