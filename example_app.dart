#!/usr/bin/env dart
/// Example Dart application demonstrating the Flow FFI bridge capabilities.
/// 
/// This example shows how to:
/// 1. Create an Environment with thread pool
/// 2. Create a Graph for computation  
/// 3. Access the NodeFactory for node management
/// 4. Create and manage nodes (limited by available node types)
/// 5. Create connections between nodes
/// 6. Execute the graph
/// 7. Handle events and errors
/// 8. Load and manage modules (if available)
/// 
/// To run this example:
/// ```bash
/// dart run example_app.dart
/// ```

import 'dart:io';
import 'dart:ffi';
import 'package:path/path.dart' as path;

// Import the flow_ffi package
import 'dart_package/lib/flow_ffi.dart';

void main() async {
  print('🚀 Flow FFI Bridge Example Application');
  print('=' * 50);
  
  try {
    await demonstrateBasicFunctionality();
    await demonstrateModuleSystem();
    await demonstrateEventSystem();
    await demonstrateErrorHandling();
  } catch (e, stackTrace) {
    print('❌ Application error: $e');
    print('Stack trace: $stackTrace');
  }
}

/// Demonstrates basic Environment, Graph, Factory, and Node operations
Future<void> demonstrateBasicFunctionality() async {
  print('\n📋 Basic Functionality Demo');
  print('-' * 30);
  
  // 1. Create Environment with 4 worker threads
  print('1. Creating Environment...');
  final env = Environment(maxThreads: 4);
  print('   ✅ Environment created: ${env.toString()}');
  
  // 2. Get the NodeFactory for node creation
  print('2. Getting NodeFactory...');
  final factory = env.factory;
  print('   ✅ NodeFactory obtained: ${factory.toString()}');
  
  // 3. Query available node categories and types
  print('3. Querying available node types...');
  try {
    final categories = factory.getCategories();
    print('   📦 Available categories: ${categories.length}');
    for (final category in categories) {
      print('      - $category');
      
      // Get node classes in each category
      final nodeClasses = factory.getNodeClasses(category);
      for (final nodeClass in nodeClasses) {
        final friendlyName = factory.getFriendlyName(nodeClass);
        print('        • $nodeClass ($friendlyName)');
      }
    }
  } catch (e) {
    print('   ⚠️  No node types available yet (need modules): $e');
  }
  
  // 4. Create a Graph for computation
  print('4. Creating Graph...');
  final graph = Graph(env);
  print('   ✅ Graph created: ${graph.toString()}');
  
  // 5. Try to create nodes (will likely fail without registered node types)
  print('5. Attempting node creation...');
  try {
    // This will fail if no node types are registered
    final node1 = graph.addNode('TestNode', 'node1');
    print('   ✅ Node created: ${node1.toString()}');
    print('      - ID: ${node1.id}');
    print('      - Name: ${node1.name}');
    print('      - Class: ${node1.className}');
    
    // Try to create another node
    final node2 = graph.addNode('ProcessorNode', 'node2');
    print('   ✅ Second node created: ${node2.toString()}');
    
    // 6. Try to connect the nodes
    print('6. Attempting to connect nodes...');
    final connection = graph.connectNodes(
      node1.id, 'output',
      node2.id, 'input',
    );
    print('   ✅ Connection created: ${connection.toString()}');
    print('      - From: ${connection.startNodeId}:${connection.startPort}');
    print('      - To: ${connection.endNodeId}:${connection.endPort}');
    
    // 7. Execute the graph
    print('7. Executing graph...');
    graph.run();
    print('   ✅ Graph execution completed');
    
  } catch (e) {
    print('   ⚠️  Node operations require registered node types: $e');
    print('   💡 This is expected without loaded modules');
  }
  
  // 8. Test serialization capabilities
  print('8. Testing graph serialization...');
  try {
    final jsonStr = graph.saveToJson();
    print('   ✅ Graph serialized to JSON (${jsonStr.length} chars)');
    print('   📄 JSON preview: ${jsonStr.substring(0, 100)}...');
  } catch (e) {
    print('   ⚠️  Graph serialization failed: $e');
  }
  
  // 9. Wait for all tasks to complete
  print('9. Waiting for background tasks...');
  env.wait();
  print('   ✅ All tasks completed');
  
  // 10. Environment variable access
  print('10. Testing environment variables...');
  final pathVar = env.getEnvironmentVariable('PATH');
  final homeVar = env.getEnvironmentVariable('HOME');
  print('   📁 PATH length: ${pathVar?.length ?? 0}');
  print('   🏠 HOME: ${homeVar ?? 'not found'}');
  
  // Clean up resources
  print('11. Cleaning up resources...');
  graph.dispose();
  factory.dispose();
  env.dispose();
  print('   ✅ Resources cleaned up');
}

/// Demonstrates the Module system capabilities
Future<void> demonstrateModuleSystem() async {
  print('\n📦 Module System Demo');
  print('-' * 25);
  
  final env = Environment(maxThreads: 2);
  final factory = env.factory;
  
  try {
    // 1. Create a Module instance
    print('1. Creating Module...');
    final module = Module(factory);
    print('   ✅ Module created: ${module.toString()}');
    
    // 2. Try to load a module (will fail without actual module files)
    print('2. Testing module loading...');
    try {
      // This path would need to contain actual .fmod files and shared libraries
      await module.load('/nonexistent/test_module');
      print('   ✅ Module loaded successfully');
      
      // 3. Check module metadata
      final metadata = module.metadata;
      if (metadata != null) {
        print('   📋 Module Metadata:');
        print('      - Name: ${metadata.name}');
        print('      - Version: ${metadata.version}');
        print('      - Author: ${metadata.author}');
        print('      - Description: ${metadata.description}');
      }
      
      // 4. Register module nodes
      module.registerNodes();
      print('   ✅ Module nodes registered');
      
      // 5. Query updated factory
      final categories = factory.getCategories();
      print('   📦 Categories after module load: ${categories.length}');
      
      // Clean up module
      module.unregisterNodes();
      await module.unload();
      
    } catch (ModuleException e) {
      print('   ⚠️  Module loading failed (expected): $e');
      print('   💡 To test modules, create a .fmod directory with:');
      print('       - module.json metadata file');
      print('       - Compiled shared library (.so/.dll/.dylib)');
    }
    
    module.dispose();
  } finally {
    factory.dispose();
    env.dispose();
  }
}

/// Demonstrates the Event system capabilities  
Future<void> demonstrateEventSystem() async {
  print('\n🔔 Event System Demo');
  print('-' * 22);
  
  final env = Environment(maxThreads: 2);
  final graph = Graph(env);
  
  try {
    // 1. Set up event listeners
    print('1. Setting up event listeners...');
    
    // Graph events
    graph.onNodeAdded.listen((event) {
      print('   🔔 Node Added: ${event.node.name}');
    });
    
    graph.onNodeRemoved.listen((event) {
      print('   🔔 Node Removed: ${event.node.name}');
    });
    
    graph.onNodesConnected.listen((event) {
      print('   🔔 Nodes Connected: ${event.connection.startNodeId} -> ${event.connection.endNodeId}');
    });
    
    graph.onNodesDisconnected.listen((event) {
      print('   🔔 Nodes Disconnected: ${event.connection.startNodeId} -X- ${event.connection.endNodeId}');
    });
    
    graph.onError.listen((event) {
      print('   🔔 Graph Error: ${event.error}');
    });
    
    print('   ✅ Event listeners registered');
    
    // 2. Try operations that would trigger events (will fail without node types)
    print('2. Testing event triggers...');
    try {
      // This should trigger onNodeAdded if it succeeds
      final node = graph.addNode('TestNode', 'event_test_node');
      
      // Set up node events
      node.onCompute.listen((event) {
        print('   🔔 Node Computed: ${event.node.name}');
      });
      
      node.onError.listen((event) {
        print('   🔔 Node Error: ${event.error}');
      });
      
      // This should trigger onCompute
      node.compute();
      
    } catch (e) {
      print('   ⚠️  Event trigger failed (no node types): $e');
    }
    
    // Give events time to propagate
    await Future.delayed(const Duration(milliseconds: 100));
    
  } finally {
    graph.dispose();
    env.dispose();
  }
}

/// Demonstrates error handling capabilities
Future<void> demonstrateErrorHandling() async {
  print('\n❌ Error Handling Demo');
  print('-' * 24);
  
  // 1. Test invalid Environment creation
  print('1. Testing invalid Environment...');
  try {
    final badEnv = Environment(maxThreads: -1);
    print('   ❌ Should have failed!');
  } catch (e) {
    print('   ✅ Correctly caught error: $e');
  }
  
  // 2. Test operations on disposed objects
  print('2. Testing disposed object access...');
  final env = Environment(maxThreads: 1);
  final graph = Graph(env);
  graph.dispose();
  
  try {
    graph.run(); // Should fail
    print('   ❌ Should have failed!');
  } catch (e) {
    print('   ✅ Correctly caught disposal error: $e');
  }
  
  // 3. Test invalid node operations
  print('3. Testing invalid node operations...');
  final validGraph = Graph(env);
  try {
    validGraph.getNode('nonexistent_id');
    print('   ❌ Should have failed!');
  } catch (e) {
    print('   ✅ Correctly caught node not found: $e');
  }
  
  // 4. Test module errors
  print('4. Testing module errors...');
  final factory = env.factory;
  final module = Module(factory);
  
  try {
    await module.load(''); // Invalid path
    print('   ❌ Should have failed!');
  } catch (ModuleException e) {
    print('   ✅ Correctly caught module error: $e');
  }
  
  // Clean up
  module.dispose();
  validGraph.dispose();
  factory.dispose();
  env.dispose();
}

/// Helper function to check if the flow_ffi library is available
bool checkLibraryAvailable() {
  try {
    // Try to find the shared library
    const libraryPaths = [
      'build/libflow_ffi.so',      // Linux
      'build/libflow_ffi.dylib',   // macOS
      'build/flow_ffi.dll',        // Windows
    ];
    
    for (final libPath in libraryPaths) {
      if (File(libPath).existsSync()) {
        print('📚 Found library: $libPath');
        return true;
      }
    }
    
    print('❌ Library not found in expected locations');
    print('💡 Run `cmake .. && make` in the build directory first');
    return false;
  } catch (e) {
    print('❌ Error checking library: $e');
    return false;
  }
}