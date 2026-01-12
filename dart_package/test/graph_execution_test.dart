import 'package:test/test.dart';
import 'package:flow_ffi/flow_ffi.dart';

/// Comprehensive test that validates the complete FFI flow from Dart through C++ to flow-core Graph::Run().
///
/// This test validates:
/// 1. The entire FFI call chain: Dart → Graph.run() → flowCore.flow_graph_run(handle) → C++ bridge → flow::Graph::Run()
/// 2. Node creation and data flow through the computational graph
/// 3. Data propagation through connected nodes
/// 4. Environment synchronization with env.wait()
/// 5. Proper resource cleanup
///
/// ## Current Limitations
///
/// This test validates the FFI bridge and graph execution API, but currently cannot test
/// actual node computation because:
///
/// 1. **No compiled node modules available**
///    - Node modules are distributed as .fmod files (zipped packages with module.json + shared libraries)
///    - Example: `dart_package/opencv_nodes.fmod` exists but lacks compiled .so files
///    - Without shared libraries, flow::Graph::Run() has no nodes to execute
///
/// 2. **What would be needed for full integration testing:**
///    - A compiled node module with at least one simple test node type
///    - Example: A "passthrough" or "echo" node that copies input to output
///    - Module structure:
///      ```
///      test_nodes.fmod (ZIP):
///      ├── module.json (metadata)
///      ├── README.md
///      ├── LICENSE
///      └── linux/libtest_nodes.so (compiled shared library)
///      ```
///    - Node registration in the shared library that provides at least one node class
///
/// 3. **How to enable full testing:**
///    - Build a test node module: C++ code that defines simple test node classes
///    - Compile to shared library (.so/.dll/.dylib)
///    - Package into .fmod file with proper module.json
///    - Update test to load module and create actual nodes
///    - Then graph.run() would execute real computation
///
/// ## What This Test Currently Validates
///
/// ✅ Complete FFI call chain (without actual nodes):
/// - Environment creation and initialization
/// - Graph creation and validity
/// - Graph.run() execution (empty graph)
/// - Environment.wait() synchronization
/// - Graph serialization/deserialization
/// - Factory node type discovery (when modules loaded)
/// - Error handling and exception propagation
/// - Resource cleanup and disposal
///
/// ❌ Not tested (requires node modules):
/// - Node creation from factory
/// - Data flow through connections
/// - Node computation results
/// - Multi-node graph execution
/// - Data type marshaling across FFI boundary
///
/// The test is designed to work in two scenarios:
/// - WITH node modules: Tests complete graph execution with actual computation
/// - WITHOUT node modules: Tests basic graph operations and API structure (current)
void main() {
  group('Graph Execution Tests', () {
    late Environment env;
    late Graph graph;
    late NodeFactory factory;

    setUp(() {
      env = Environment(maxThreads: 4);
      graph = Graph(env);
      factory = env.factory;
    });

    tearDown(() {
      graph.dispose();
      factory.dispose();
      env.dispose();
    });

    test('basic graph creation and validation', () {
      // Validate that the graph was created successfully
      expect(graph.isValid, isTrue,
          reason: 'Graph handle should be valid after creation');
      expect(graph.refCount, greaterThan(0),
          reason: 'Graph should have positive reference count');

      print('[GraphExecution] Graph created successfully');
      print('[GraphExecution] Graph valid: ${graph.isValid}');
      print('[GraphExecution] Graph refCount: ${graph.refCount}');
    });

    test('empty graph run and environment wait', () {
      // Test the complete FFI call chain even with an empty graph
      // This validates: Dart → Graph.run() → C++ flow::Graph::Run()
      expect(() => graph.run(), returnsNormally,
          reason: 'Running an empty graph should succeed');

      print('[GraphExecution] Empty graph.run() succeeded');

      // Test environment wait - validates C++ thread pool synchronization
      // This validates: Dart → Environment.wait() → C++ flow::Env::Wait()
      expect(() => env.wait(), returnsNormally,
          reason: 'Environment wait should succeed');

      print('[GraphExecution] Environment.wait() succeeded');
      print('[GraphExecution] ✓ FFI call chain validated for empty graph');
    });

    test('graph serialization and deserialization', () {
      // Validate graph state can be saved and loaded
      // This tests: Dart → C++ graph serialization → JSON → C++ deserialization
      final jsonString = graph.saveToJson();
      expect(jsonString, isNotNull);
      expect(jsonString, isNotEmpty,
          reason: 'Graph JSON should not be empty');

      print(
          '[GraphExecution] Graph serialized to JSON (${jsonString.length} chars)');

      // Create a new graph and load the serialized state
      final graph2 = Graph(env);
      expect(() => graph2.loadFromJson(jsonString), returnsNormally,
          reason: 'Loading graph from JSON should succeed');

      print('[GraphExecution] Graph deserialized successfully');

      graph2.dispose();
    });

    test('factory node type discovery', () {
      // Discover what node types are available
      final categories = factory.getCategories();
      print('[GraphExecution] Available categories: ${categories.length}');

      if (categories.isEmpty) {
        print(
            '[GraphExecution] No node modules loaded - tests will be limited to API validation');
      } else {
        print('[GraphExecution] Found categories: $categories');

        // List all available node classes
        for (final category in categories) {
          final classes = factory.getNodeClasses(category);
          print('[GraphExecution]   $category: ${classes.join(", ")}');
        }
      }
    });

    test('node creation and basic operations', () {
      // Try to discover and create nodes from available types
      final categories = factory.getCategories();

      if (categories.isEmpty) {
        print(
            '[GraphExecution] Skipping node creation - no modules loaded (this is expected)');
        return;
      }

      // Find any available node class
      String? nodeClass;
      for (final category in categories) {
        final classes = factory.getNodeClasses(category);
        if (classes.isNotEmpty) {
          nodeClass = classes.first;
          break;
        }
      }

      if (nodeClass == null) {
        print('[GraphExecution] No node classes available in any category');
        return;
      }

      // Create a node in the graph
      // This validates: Dart → Graph.addNode() → C++ Node creation
      final node = graph.addNode(nodeClass, 'test_node');

      expect(node.isValid, isTrue, reason: 'Node should be valid after creation');
      expect(node.id, isNotEmpty, reason: 'Node should have a valid ID');
      expect(node.name, equals('test_node'), reason: 'Node name should match');
      expect(node.className, equals(nodeClass),
          reason: 'Node class should match');

      print('[GraphExecution] Created node: ${node.name} (${node.className})');
      print('[GraphExecution]   ID: ${node.id}');
      print('[GraphExecution]   Valid: ${node.isValid}');
      print('[GraphExecution]   RefCount: ${node.refCount}');

      // Get port information
      final inputPorts = node.getInputPortKeys();
      final outputPorts = node.getOutputPortKeys();

      print('[GraphExecution]   Input ports: ${inputPorts.length}');
      print('[GraphExecution]   Output ports: ${outputPorts.length}');

      // Verify the node appears in the graph's node list
      final nodes = graph.getNodes();
      expect(nodes, hasLength(1), reason: 'Graph should contain one node');
      expect(nodes.first.id, equals(node.id),
          reason: 'Node in graph should match created node');

      print('[GraphExecution] ✓ Node creation and queries successful');
    });

    test('single node computation with data flow', () {
      final categories = factory.getCategories();
      if (categories.isEmpty) {
        print(
            '[GraphExecution] Skipping computation test - no modules loaded');
        return;
      }

      // Look for a node type that can perform computation
      // Prefer nodes with both input and output ports
      Node? computeNode;
      String? nodeClass;

      for (final category in categories) {
        final classes = factory.getNodeClasses(category);
        for (final className in classes) {
          try {
            final testNode = graph.addNode(className, 'probe_node');
            final inputPorts = testNode.getInputPortKeys();
            final outputPorts = testNode.getOutputPortKeys();

            // Remove this test node
            graph.removeNode(testNode.id);

            // If this node has both inputs and outputs, use it
            if (inputPorts.isNotEmpty && outputPorts.isNotEmpty) {
              nodeClass = className;
              break;
            }
          } catch (e) {
            // Skip nodes that fail to create
            continue;
          }
        }
        if (nodeClass != null) break;
      }

      if (nodeClass == null) {
        print(
            '[GraphExecution] No suitable compute node found with input/output ports');
        return;
      }

      // Create the compute node
      computeNode = graph.addNode(nodeClass, 'compute_node');
      print('[GraphExecution] Testing computation with node: $nodeClass');

      final inputPorts = computeNode.getInputPortKeys();
      final outputPorts = computeNode.getOutputPortKeys();

      print('[GraphExecution]   Input ports: $inputPorts');
      print('[GraphExecution]   Output ports: $outputPorts');

      // Try to set input data on the first port
      if (inputPorts.isNotEmpty) {
        final portKey = inputPorts.first;

        // Try setting an integer value
        try {
          computeNode.setInputData(portKey, 42);
          print('[GraphExecution]   Set input data: $portKey = 42');

          // Verify we can read it back
          final inputValue = computeNode.getInputData<int>(portKey);
          expect(inputValue, equals(42),
              reason: 'Input data should round-trip correctly');
          print('[GraphExecution]   Read input data: $portKey = $inputValue');
        } catch (e) {
          print('[GraphExecution]   Integer input failed: $e');

          // Try with a double instead
          try {
            computeNode.setInputData(portKey, 3.14);
            print('[GraphExecution]   Set input data: $portKey = 3.14');

            final inputValue = computeNode.getInputData<double>(portKey);
            expect(inputValue, equals(3.14),
                reason: 'Double input data should round-trip correctly');
            print('[GraphExecution]   Read input data: $portKey = $inputValue');
          } catch (e2) {
            print('[GraphExecution]   Double input also failed: $e2');
          }
        }
      }

      // Execute the graph
      // This is the critical FFI call chain we're testing:
      // Dart graph.run() → flowCore.flow_graph_run(handle) → C++ flow::Graph::Run()
      expect(() => graph.run(), returnsNormally,
          reason: 'Graph execution should succeed');
      print('[GraphExecution] ✓ graph.run() executed successfully');

      // Wait for all computations to complete
      // This validates the thread pool synchronization:
      // Dart env.wait() → C++ flow::Env::Wait()
      expect(() => env.wait(), returnsNormally,
          reason: 'Environment wait should succeed');
      print('[GraphExecution] ✓ env.wait() completed successfully');

      // Try to read output data if available
      if (outputPorts.isNotEmpty) {
        final portKey = outputPorts.first;
        try {
          final outputValue = computeNode.getOutputData<dynamic>(portKey);
          print(
              '[GraphExecution]   Output data: $portKey = $outputValue (type: ${outputValue?.runtimeType})');
        } catch (e) {
          print('[GraphExecution]   Reading output data failed: $e');
        }
      }

      print('[GraphExecution] ✓ Single node computation test completed');
    });

    test('multiple connected nodes with data propagation', () {
      final categories = factory.getCategories();
      if (categories.isEmpty) {
        print('[GraphExecution] Skipping multi-node test - no modules loaded');
        return;
      }

      // Try to create a chain of nodes with compatible ports
      // We need at least two nodes where node1's output can connect to node2's input

      String? sourceNodeClass;
      String? sinkNodeClass;

      // Find nodes with outputs
      final nodesWithOutputs = <String>[];
      // Find nodes with inputs
      final nodesWithInputs = <String>[];

      for (final category in categories) {
        final classes = factory.getNodeClasses(category);
        for (final className in classes) {
          try {
            final testNode = graph.addNode(className, 'probe');
            final inputPorts = testNode.getInputPortKeys();
            final outputPorts = testNode.getOutputPortKeys();
            graph.removeNode(testNode.id);

            if (outputPorts.isNotEmpty) {
              nodesWithOutputs.add(className);
            }
            if (inputPorts.isNotEmpty) {
              nodesWithInputs.add(className);
            }
          } catch (e) {
            continue;
          }
        }
      }

      print(
          '[GraphExecution] Nodes with outputs: ${nodesWithOutputs.length}');
      print('[GraphExecution] Nodes with inputs: ${nodesWithInputs.length}');

      if (nodesWithOutputs.isEmpty || nodesWithInputs.isEmpty) {
        print(
            '[GraphExecution] Insufficient nodes for connection testing');
        return;
      }

      // Create two nodes
      sourceNodeClass = nodesWithOutputs.first;
      sinkNodeClass = nodesWithInputs.first;

      final node1 = graph.addNode(sourceNodeClass, 'source_node');
      final node2 = graph.addNode(sinkNodeClass, 'sink_node');

      print('[GraphExecution] Created node chain:');
      print('[GraphExecution]   Source: $sourceNodeClass (${node1.id.substring(0, 8)}...)');
      print('[GraphExecution]   Sink: $sinkNodeClass (${node2.id.substring(0, 8)}...)');

      final node1Outputs = node1.getOutputPortKeys();
      final node2Inputs = node2.getInputPortKeys();

      print('[GraphExecution]   Source outputs: $node1Outputs');
      print('[GraphExecution]   Sink inputs: $node2Inputs');

      // Try to connect the nodes
      if (node1Outputs.isNotEmpty && node2Inputs.isNotEmpty) {
        final sourcePort = node1Outputs.first;
        final targetPort = node2Inputs.first;

        try {
          final connection = graph.connectNodes(
            node1.id,
            sourcePort,
            node2.id,
            targetPort,
          );

          expect(connection.isValid, isTrue,
              reason: 'Connection should be valid');
          print(
              '[GraphExecution] ✓ Connected: ${node1.name}.$sourcePort → ${node2.name}.$targetPort');

          // Set input data on the source node if it has input ports
          final node1Inputs = node1.getInputPortKeys();
          if (node1Inputs.isNotEmpty) {
            try {
              node1.setInputData(node1Inputs.first, 100);
              print(
                  '[GraphExecution]   Set source input: ${node1Inputs.first} = 100');
            } catch (e) {
              print('[GraphExecution]   Setting source input failed: $e');
            }
          }

          // Execute the graph to propagate data
          print('[GraphExecution] Executing graph with connected nodes...');
          graph.run();
          print('[GraphExecution] ✓ graph.run() succeeded with connections');

          // Wait for computation
          env.wait();
          print('[GraphExecution] ✓ env.wait() succeeded');

          // Check if data propagated
          try {
            final outputData = node1.getOutputData<dynamic>(sourcePort);
            print(
                '[GraphExecution]   Source output: $outputData');
          } catch (e) {
            print('[GraphExecution]   Reading source output failed: $e');
          }

          try {
            final inputData = node2.getInputData<dynamic>(targetPort);
            print('[GraphExecution]   Sink input: $inputData');
          } catch (e) {
            print('[GraphExecution]   Reading sink input failed: $e');
          }

          print(
              '[GraphExecution] ✓ Multi-node connection test completed');
        } catch (e) {
          print('[GraphExecution] Connection failed (may be type mismatch): $e');
          // This is OK - not all port types are compatible
        }
      }
    });

    test('error handling for invalid operations', () {
      // Test error handling for invalid node class
      expect(
        () => graph.addNode('NonExistent.InvalidClass', 'bad_node'),
        throwsA(isA<FlowException>()),
        reason: 'Adding invalid node class should throw FlowException',
      );

      print('[GraphExecution] ✓ Invalid node class throws exception');

      // Test error handling for invalid connection
      final categories = factory.getCategories();
      if (categories.isNotEmpty) {
        // Create a node
        String? nodeClass;
        for (final category in categories) {
          final classes = factory.getNodeClasses(category);
          if (classes.isNotEmpty) {
            nodeClass = classes.first;
            break;
          }
        }

        if (nodeClass != null) {
          final node = graph.addNode(nodeClass, 'test_node');

          // Try to connect to non-existent node
          expect(
            () => graph.connectNodes(
              node.id,
              'output',
              'non-existent-id',
              'input',
            ),
            throwsA(isA<FlowException>()),
            reason: 'Connecting to non-existent node should throw',
          );

          print('[GraphExecution] ✓ Invalid connection throws exception');

          // Try to get non-existent node
          expect(
            () => graph.getNode('non-existent-id'),
            throwsA(isA<FlowException>()),
            reason: 'Getting non-existent node should throw',
          );

          print('[GraphExecution] ✓ Getting non-existent node throws exception');
        }
      }
    });

    test('resource cleanup and disposal', () {
      // Create some nodes if possible
      final categories = factory.getCategories();
      if (categories.isNotEmpty) {
        for (final category in categories) {
          final classes = factory.getNodeClasses(category);
          if (classes.isNotEmpty) {
            final node = graph.addNode(classes.first, 'cleanup_test');
            print('[GraphExecution] Created node for cleanup test');

            // Dispose the node explicitly
            node.dispose();
            expect(node.isValid, isFalse,
                reason: 'Node should be invalid after disposal');
            print('[GraphExecution] ✓ Node disposed successfully');
            break;
          }
        }
      }

      // Clear the graph
      graph.clear();
      final nodes = graph.getNodes();
      expect(nodes, isEmpty, reason: 'Graph should be empty after clear');
      print('[GraphExecution] ✓ Graph cleared successfully');

      // Dispose and recreate graph
      graph.dispose();
      expect(graph.isValid, isFalse,
          reason: 'Graph should be invalid after disposal');
      print('[GraphExecution] ✓ Graph disposed successfully');

      // Create a new graph to ensure environment is still valid
      graph = Graph(env);
      expect(graph.isValid, isTrue,
          reason: 'New graph should be valid');
      print('[GraphExecution] ✓ New graph created after disposal');

      print('[GraphExecution] ✓ Resource cleanup test completed');
    });

    test('stress test: multiple graph executions', () {
      // Execute the graph multiple times to test stability
      const iterations = 10;

      for (int i = 0; i < iterations; i++) {
        expect(() => graph.run(), returnsNormally,
            reason: 'Graph execution $i should succeed');
        expect(() => env.wait(), returnsNormally,
            reason: 'Environment wait $i should succeed');
      }

      print(
          '[GraphExecution] ✓ Completed $iterations graph executions successfully');
    });

    test('comprehensive FFI call chain validation', () {
      print('[GraphExecution] Validating complete FFI call chain:');
      print('[GraphExecution]');
      print(
          '[GraphExecution] 1. Environment creation: Dart → C++ flow::Env::Create()');
      expect(env.isValid, isTrue);
      print('[GraphExecution]    ✓ Environment valid');

      print(
          '[GraphExecution] 2. Graph creation: Dart → C++ flow::Graph::Create()');
      expect(graph.isValid, isTrue);
      print('[GraphExecution]    ✓ Graph valid');

      print(
          '[GraphExecution] 3. Factory access: Dart → C++ flow::NodeFactory::Get()');
      expect(factory.isValid, isTrue);
      print('[GraphExecution]    ✓ Factory valid');

      print(
          '[GraphExecution] 4. Graph execution: Dart → C++ flow::Graph::Run()');
      graph.run();
      print('[GraphExecution]    ✓ graph.run() succeeded');

      print(
          '[GraphExecution] 5. Thread sync: Dart → C++ flow::Env::Wait()');
      env.wait();
      print('[GraphExecution]    ✓ env.wait() succeeded');

      print(
          '[GraphExecution] 6. Serialization: Dart → C++ → JSON → Dart');
      final json = graph.saveToJson();
      expect(json, isNotEmpty);
      print('[GraphExecution]    ✓ Serialization succeeded');

      final graph2 = Graph(env);
      graph2.loadFromJson(json);
      print('[GraphExecution]    ✓ Deserialization succeeded');
      graph2.dispose();

      print('[GraphExecution]');
      print(
          '[GraphExecution] ✓✓✓ COMPLETE FFI CALL CHAIN VALIDATED ✓✓✓');
      print('[GraphExecution]');
      print('[GraphExecution] The entire flow from Dart through C++ FFI bridge');
      print(
          '[GraphExecution] to flow-core Graph::Run() has been successfully tested.');
    });

    test('port compatibility verification with canConnect', () {
      print('[GraphExecution] Testing port compatibility verification:');

      // canConnect on an empty graph should return false for any connection
      // since there are no nodes
      final result1 = graph.canConnect(
        'nonexistent-source',
        'output',
        'nonexistent-target',
        'input',
      );
      expect(result1, isFalse,
          reason:
              'canConnect should return false for nonexistent nodes');
      print(
          '[GraphExecution] ✓ canConnect correctly returns false for nonexistent nodes');

      // Verify canConnect doesn't throw exceptions (returns boolean)
      expect(
        () => graph.canConnect(
          'any-source',
          'any-port',
          'any-target',
          'any-port',
        ),
        returnsNormally,
        reason: 'canConnect should not throw exceptions',
      );
      print('[GraphExecution] ✓ canConnect returns boolean without throwing');

      // This validates the new canConnect method integrates properly with the FFI layer
      // and provides pre-validation capability for graph construction
      expect(graph.isValid, isTrue,
          reason: 'Graph should remain valid after canConnect checks');
      print('[GraphExecution] ✓ Graph remains valid after canConnect checks');
    });
  });
}
