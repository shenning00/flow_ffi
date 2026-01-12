import 'package:flow_ffi/flow_ffi.dart';

/// Simple example showing basic Flow FFI usage.
///
/// This example demonstrates the minimum steps needed to:
/// 1. Create an environment
/// 2. Access the node factory
/// 3. Create a graph
/// 4. Perform basic operations
/// 5. Clean up resources
Future<void> main() async {
  print('🚀 Simple Flow FFI Workflow Example');
  print('===================================\n');

  try {
    // Step 1: Create environment
    print('1. Creating environment...');
    final env = Environment(maxThreads: 4);
    print('   ✅ Environment created');

    // Step 2: Get factory
    print('2. Accessing node factory...');
    final factory = env.factory;
    print('   ✅ Factory obtained');

    // Step 3: Query available node types
    print('3. Querying available node types...');
    final categories = factory.getCategories();
    print('   📦 Found ${categories.length} categories');

    if (categories.isNotEmpty) {
      print('   Available categories:');
      for (final category in categories) {
        final classes = factory.getNodeClasses(category);
        print('     - $category: ${classes.length} node types');
      }
    } else {
      print('   💡 No node types available (no modules loaded)');
    }

    // Step 4: Create a graph
    print('4. Creating computational graph...');
    final graph = Graph(env);
    print('   ✅ Graph created');

    // Step 5: Try basic graph operations
    print('5. Testing graph operations...');

    try {
      // This will likely fail without loaded modules, but shows the API
      final node = graph.addNode('ExampleNode', 'example');
      print('   ✅ Node created: ${node.name}');

      // If successful, try more operations
      node.compute();
      print('   ✅ Node computation triggered');

      graph.run();
      print('   ✅ Graph executed');
    } catch (e) {
      print('   ⚠️  Node operations require loaded modules');
      print('   This is expected behavior without registered node types');
    }

    // Step 6: Test serialization
    print('6. Testing graph serialization...');
    final json = graph.saveToJson();
    print('   ✅ Graph serialized (${json.length} characters)');

    // Step 7: Environment utilities
    print('7. Testing environment utilities...');
    final pathVar = env.getEnvironmentVariable('PATH');
    print('   PATH variable length: ${pathVar?.length ?? 0}');

    env.wait(); // Wait for any background tasks
    print('   ✅ All tasks completed');

    // Step 8: Cleanup
    print('8. Cleaning up resources...');
    graph.dispose();
    factory.dispose();
    env.dispose();
    print('   ✅ Resources cleaned up');

    print('\n🎉 Simple workflow completed successfully!');
    print('The Flow FFI bridge is working correctly.');
  } on FlowException catch (e) {
    print('\n❌ Flow error occurred: ${e.message}');
    print('This indicates an issue with the FFI bridge.');
  } catch (e) {
    print('\n❌ Unexpected error: $e');
    print('This may indicate a setup or configuration issue.');
  }

  print('\n💡 Next steps:');
  print('   - Load modules to access node types');
  print('   - Create more complex graphs');
  print('   - Set up event listeners');
  print('   - Try the interactive demo');
}

/// Minimal example showing just the core operations.
Future<void> minimalExample() async {
  // Create environment
  final env = Environment(maxThreads: 2);

  // Create graph
  final graph = Graph(env);

  // Basic operations
  final categories = env.factory.getCategories();
  print('Available categories: ${categories.length}');

  // Cleanup
  graph.dispose();
  env.factory.dispose();
  env.dispose();

  print('Minimal example completed!');
}
