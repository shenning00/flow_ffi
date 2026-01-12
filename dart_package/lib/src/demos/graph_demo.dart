import '../services/flow_service.dart';
import '../services/graph_builder.dart';
import '../models/graph.dart';
import '../models/environment.dart';
import '../utils/error_handler.dart';

/// Demonstrates graph creation and manipulation.
///
/// This demo shows how to create graphs, add nodes, make connections,
/// and execute computational workflows using both direct API calls
/// and the fluent GraphBuilder interface.
Future<void> runGraphDemo() async {
  print('📊 Flow FFI Graph Demo');
  print('======================');

  final service = FlowService();

  try {
    // Initialize the service
    print('\n1. Initializing Flow Service...');
    await service.initialize(maxThreads: 4);
    print('✅ Service initialized');

    // Basic graph operations
    await _demonstrateBasicGraphOps(service);

    // GraphBuilder fluent API
    await _demonstrateGraphBuilder(service);

    // Graph serialization
    await _demonstrateGraphSerialization(service);

    // Advanced graph operations
    await _demonstrateAdvancedOps(service);
  } on FlowException catch (e) {
    print('\n❌ Flow error: $e');
  } catch (e) {
    print('\n❌ Unexpected error: $e');
  } finally {
    print('\n8. Cleaning up...');
    await service.cleanup();
    print('🧹 Service cleaned up');
  }

  print('\n✅ Graph Demo completed!');
}

/// Demonstrates basic graph operations.
Future<void> _demonstrateBasicGraphOps(FlowService service) async {
  print('\n2. Basic Graph Operations...');

  try {
    // Create a graph
    final graph = await service.createGraph();
    print('✅ Graph created');
    print('   Graph valid: ${graph.isValid}');
    print('   Reference count: ${graph.refCount}');

    // Try to add nodes (will fail without registered node types)
    print('\n   Attempting to add nodes...');
    try {
      final node1 = graph.addNode('InputNode', 'input1');
      print('✅ Node created: ${node1.name} (${node1.id.substring(0, 8)}...)');

      final node2 = graph.addNode('ProcessorNode', 'processor1');
      print('✅ Node created: ${node2.name} (${node2.id.substring(0, 8)}...)');

      // Try to connect them
      final connection = graph.connectNodes(
        node1.id,
        'output',
        node2.id,
        'input',
      );
      print('✅ Nodes connected: ${connection.description}');

      // Execute the graph
      graph.run();
      print('✅ Graph executed successfully');
    } on NodeNotFoundException catch (e) {
      print('⚠️  Node creation failed (expected without modules): $e');
      print('💡 This is normal when no node modules are loaded');
    } catch (e) {
      print('⚠️  Graph operations failed: $e');
    }

    // Test graph queries
    final nodes = graph.getNodes();
    print('   Total nodes in graph: ${nodes.length}');
  } catch (e) {
    print('❌ Basic graph operations failed: $e');
  }
}

/// Demonstrates the GraphBuilder fluent API.
Future<void> _demonstrateGraphBuilder(FlowService service) async {
  print('\n3. GraphBuilder Fluent API...');

  try {
    final graph = await service.createGraph();
    final builder = GraphBuilder(graph);

    print('✅ GraphBuilder created');

    // Try to build a workflow with fluent API
    try {
      builder
        ..addNode('input', 'DataSource', 'Input Data')
        ..addNode('transform', 'Transformer', 'Data Transformer')
        ..addNode('output', 'DataSink', 'Output Sink')
        ..connect('input', 'data', 'transform', 'input')
        ..connect('transform', 'result', 'output', 'input');

      print('✅ Workflow built with fluent API');
      print(
          '   ${builder.nodes.length} nodes, ${builder.connections.length} connections');

      // Try to execute
      builder.execute();
      print('✅ Workflow executed');
    } on NodeNotFoundException catch (e) {
      print('⚠️  Workflow building failed (expected without modules): $e');

      // Show what we can do without actual node types
      print('\n   Demonstrating builder capabilities without real nodes:');

      // Clear and show validation
      builder.clear();
      final summary = builder.getSummary();
      print('   Empty graph summary: $summary');
    } catch (e) {
      print('⚠️  GraphBuilder operations failed: $e');
    }
  } catch (e) {
    print('❌ GraphBuilder demonstration failed: $e');
  }
}

/// Demonstrates graph serialization.
Future<void> _demonstrateGraphSerialization(FlowService service) async {
  print('\n4. Graph Serialization...');

  try {
    final graph = await service.createGraph();

    // Try to save empty graph
    print('   Testing empty graph serialization...');
    final emptyJson = graph.saveToJson();
    print('✅ Empty graph serialized (${emptyJson.length} chars)');

    // Try to load it back
    final graph2 = await service.createGraph();
    graph2.loadFromJson(emptyJson);
    print('✅ Empty graph deserialized');

    // Test JSON object format
    final jsonObject = graph.saveToJsonObject();
    print('   JSON object keys: ${jsonObject.keys.join(', ')}');
  } catch (e) {
    print('❌ Graph serialization failed: $e');
  }
}

/// Demonstrates advanced graph operations.
Future<void> _demonstrateAdvancedOps(FlowService service) async {
  print('\n5. Advanced Graph Operations...');

  try {
    final graph = await service.createGraph();

    // Test graph clearing
    print('   Testing graph clear...');
    graph.clear();
    final nodes = graph.getNodes();
    print('✅ Graph cleared, nodes: ${nodes.length}');

    // Test multiple graphs
    print('   Testing multiple graphs...');
    final graphs = <Graph>[];
    for (int i = 0; i < 3; i++) {
      final g = await service.createGraph();
      graphs.add(g);
    }
    print('✅ Created ${graphs.length} graphs');

    // Cleanup additional graphs
    for (final g in graphs) {
      g.dispose();
    }
    print('✅ Additional graphs disposed');
  } catch (e) {
    print('❌ Advanced operations failed: $e');
  }
}

/// Runs a stress test on graph operations.
Future<void> runGraphStressTest(
    {int graphCount = 10, int maxThreads = 4}) async {
  print('🔥 Graph Stress Test');
  print('===================');
  print('Creating $graphCount graphs with $maxThreads threads each...');

  final stopwatch = Stopwatch()..start();
  final services = <FlowService>[];
  final graphs = <Graph>[];
  late final Stopwatch opStopwatch;

  try {
    // Create multiple services and graphs
    for (int i = 0; i < graphCount; i++) {
      final service = FlowService();
      await service.initialize(maxThreads: maxThreads);
      services.add(service);

      final graph = await service.createGraph();
      graphs.add(graph);

      if ((i + 1) % 5 == 0) {
        print('   Created ${i + 1}/$graphCount graphs...');
      }
    }

    stopwatch.stop();
    print('✅ Created $graphCount graphs in ${stopwatch.elapsedMilliseconds}ms');
    print(
        '   Average: ${stopwatch.elapsedMilliseconds / graphCount}ms per graph');

    // Test operations on all graphs
    print('\n   Testing operations on all graphs...');
    opStopwatch = Stopwatch()..start();

    for (int i = 0; i < graphs.length; i++) {
      final graph = graphs[i];

      try {
        // Basic operations
        final nodes = graph.getNodes();
        final json = graph.saveToJson();

        // Each graph should be independent
        assert(nodes.isEmpty); // Should be empty
        assert(json.isNotEmpty); // Should have some JSON content
      } catch (e) {
        print('   Warning: Graph $i operation failed: $e');
      }
    }

    opStopwatch.stop();
    print(
        '✅ Tested operations on $graphCount graphs in ${opStopwatch.elapsedMilliseconds}ms');
  } catch (e) {
    print('❌ Stress test failed: $e');
  } finally {
    // Cleanup everything
    print('\n   Cleaning up stress test resources...');
    final cleanupStopwatch = Stopwatch()..start();

    for (final graph in graphs) {
      try {
        graph.dispose();
      } catch (e) {
        print('   Warning: Failed to dispose graph: $e');
      }
    }

    for (final service in services) {
      try {
        await service.cleanup();
      } catch (e) {
        print('   Warning: Failed to cleanup service: $e');
      }
    }

    cleanupStopwatch.stop();
    print('✅ Cleanup completed in ${cleanupStopwatch.elapsedMilliseconds}ms');

    print('\n📊 Stress Test Results:');
    print('   Graph creation: ${stopwatch.elapsedMilliseconds}ms total');
    print('   Operations: ${opStopwatch.elapsedMilliseconds}ms total');
    print('   Cleanup: ${cleanupStopwatch.elapsedMilliseconds}ms total');
    print('   Peak graphs: $graphCount');
    print(
        '   Success rate: ${graphs.length}/$graphCount (${(graphs.length / graphCount * 100).toStringAsFixed(1)}%)');
  }
}
