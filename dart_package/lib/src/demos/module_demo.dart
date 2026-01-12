import 'dart:io';

import '../services/flow_service.dart';
import '../models/module.dart';
import '../models/environment.dart';
import '../models/factory.dart';
import '../utils/error_handler.dart';

/// Demonstrates module loading and dynamic node registration.
///
/// This demo shows how to load modules from .fmod directories,
/// access module metadata, register node types, and use the
/// dynamically loaded nodes in graphs.
Future<void> runModuleDemo() async {
  print('📦 Flow FFI Module Demo');
  print('=======================');

  final service = FlowService();

  try {
    print('\n1. Initializing Service...');
    await service.initialize(maxThreads: 4);
    print('✅ Service initialized');

    final factory = service.factory!;

    // Check initial state
    await _checkInitialState(factory);

    // Attempt to load test modules
    await _attemptModuleLoading(factory);

    // Demonstrate module operations
    await _demonstrateModuleOperations(factory);

    // Test module lifecycle
    await _testModuleLifecycle(factory);

    // Advanced module testing
    await _testAdvancedModuleFeatures(factory);
  } on FlowException catch (e) {
    print('\n❌ Flow error: $e');
  } catch (e) {
    print('\n❌ Unexpected error: $e');
  } finally {
    await service.cleanup();
    print('\n🧹 Service cleaned up');
  }

  print('\n✅ Module Demo completed!');
}

/// Checks the initial state before loading any modules.
Future<void> _checkInitialState(NodeFactory factory) async {
  print('\n2. Checking Initial State...');

  try {
    final initialCategories = factory.getCategories();
    print('   Initial categories: ${initialCategories.length}');

    if (initialCategories.isNotEmpty) {
      print('   Found existing categories:');
      for (final category in initialCategories) {
        final classes = factory.getNodeClasses(category);
        print('     - $category: ${classes.length} classes');
        for (final className in classes.take(3)) {
          print('       • $className');
        }
        if (classes.length > 3) {
          print('       • ... and ${classes.length - 3} more');
        }
      }
    } else {
      print('   No categories found (clean start)');
    }
  } catch (e) {
    print('❌ Failed to check initial state: $e');
  }
}

/// Attempts to load modules from various locations.
Future<void> _attemptModuleLoading(NodeFactory factory) async {
  print('\n3. Attempting Module Loading...');

  final module = Module(factory);
  print('✅ Module instance created');
  print('   Module valid: ${module.isValid}');

  // Test common module locations
  final testPaths = [
    './test_module',
    '../test_module',
    '../../test_module',
    './modules/basic_nodes',
    '../modules/basic_nodes',
    '/tmp/test_module',
  ];

  bool foundModule = false;

  for (final path in testPaths) {
    print('\n   Trying to load module from: $path');

    try {
      await module.load(path);

      if (module.isLoaded) {
        print('✅ Module loaded successfully!');
        foundModule = true;

        // Show module metadata
        final metadata = module.metadata;
        if (metadata != null) {
          print('   📋 Module Metadata:');
          print('      Name: ${metadata.name}');
          print('      Version: ${metadata.version}');
          print('      Author: ${metadata.author}');
          print('      Description: ${metadata.description}');
        } else {
          print('   ⚠️  No metadata available');
        }

        break;
      }
    } on ModuleException catch (e) {
      print('   ❌ Failed: ${e.message}');
    } catch (e) {
      print('   ❌ Unexpected error: $e');
    }
  }

  if (!foundModule) {
    print('\n   💡 No modules found at test locations.');
    print('   This is expected if no test modules are available.');
    print('   To test module loading:');
    print('     1. Create a directory with .fmod.json metadata');
    print('     2. Include a shared library (.so/.dll/.dylib)');
    print('     3. Provide the path to this demo');
  }

  // Test module creation without loading
  print('\n   Testing module creation and basic operations...');
  try {
    final testModule = Module(factory);
    print('   ✅ Test module created');

    final isLoaded = testModule.isLoaded;
    print('   Module loaded state: $isLoaded');

    // Try metadata access on unloaded module
    try {
      final metadata = testModule.metadata;
      print('   Metadata on unloaded module: ${metadata != null}');
    } catch (e) {
      print('   Metadata access failed (expected): ${e.runtimeType}');
    }

    testModule.dispose();
    print('   ✅ Test module disposed');
  } catch (e) {
    print('   ❌ Module creation test failed: $e');
  }
}

/// Demonstrates module operations with a loaded module.
Future<void> _demonstrateModuleOperations(NodeFactory factory) async {
  print('\n4. Module Operations...');

  final module = Module(factory);

  try {
    // For demonstration, we'll show what operations are available
    // even if we don't have a real module to load

    print('   Available module operations:');
    print('     - load(path): Load module from directory');
    print('     - unload(): Unload currently loaded module');
    print('     - registerNodes(): Register module nodes with factory');
    print('     - unregisterNodes(): Remove module nodes from factory');
    print('     - isLoaded: Check if module is loaded');
    print('     - metadata: Access module information');

    // Test the module state queries
    print('   Current module state:');
    print('     Loaded: ${module.isLoaded}');
    print('     Valid: ${module.isValid}');
    print('     RefCount: ${module.refCount}');

    // Show what happens when we try operations on unloaded module
    print('\n   Testing operations on unloaded module...');

    try {
      module.registerNodes();
      print('   ⚠️  registerNodes() succeeded (unexpected)');
    } catch (e) {
      print('   ✅ registerNodes() failed as expected: ${e.runtimeType}');
    }

    try {
      module.unregisterNodes();
      print('   ⚠️  unregisterNodes() succeeded (unexpected)');
    } catch (e) {
      print('   ✅ unregisterNodes() failed as expected: ${e.runtimeType}');
    }
  } catch (e) {
    print('❌ Module operations test failed: $e');
  } finally {
    module.dispose();
  }
}

/// Tests the complete module lifecycle.
Future<void> _testModuleLifecycle(NodeFactory factory) async {
  print('\n5. Module Lifecycle Testing...');

  try {
    // Test multiple module instances
    final modules = <Module>[];

    print('   Creating multiple module instances...');
    for (int i = 0; i < 3; i++) {
      final module = Module(factory);
      modules.add(module);
      print('   Module $i: valid=${module.isValid}, loaded=${module.isLoaded}');
    }

    print('   ✅ Created ${modules.length} module instances');

    // Test concurrent operations
    print('   Testing concurrent module operations...');
    final futures = modules.asMap().entries.map((entry) async {
      final index = entry.key;
      final module = entry.value;
      try {
        // Try to load from non-existent path (should fail gracefully)
        await module.load('/non/existent/path/module${index}_test');
        return 'loaded';
      } catch (e) {
        return 'failed: ${e.runtimeType}';
      }
    }).toList();

    final results = await Future.wait(futures);
    for (int i = 0; i < results.length; i++) {
      print('   Module $i result: ${results[i]}');
    }

    // Cleanup all modules
    print('   Cleaning up modules...');
    for (int i = 0; i < modules.length; i++) {
      try {
        modules[i].dispose();
        print('   Module $i disposed');
      } catch (e) {
        print('   Module $i disposal failed: $e');
      }
    }

    print('✅ Module lifecycle test completed');
  } catch (e) {
    print('❌ Module lifecycle test failed: $e');
  }
}

/// Tests advanced module features and edge cases.
Future<void> _testAdvancedModuleFeatures(NodeFactory factory) async {
  print('\n6. Advanced Module Features...');

  try {
    // Test module with very long paths
    print('   Testing edge cases...');

    final module = Module(factory);

    final edgeCases = [
      '', // Empty path
      '/', // Root path
      '/dev/null', // Invalid file
      'a' * 1000, // Very long path
      '/path/with spaces/module', // Path with spaces
      '/path/with/ünicöde/mödule', // Unicode path
    ];

    for (int i = 0; i < edgeCases.length; i++) {
      final path = edgeCases[i];
      print(
          '   Edge case ${i + 1}: ${path.length > 50 ? '${path.substring(0, 47)}...' : path}');

      try {
        await module.load(path);
        if (module.isLoaded) {
          print('     ⚠️  Unexpectedly loaded');
          await module.unload();
        } else {
          print('     ✅ Failed to load (expected)');
        }
      } catch (e) {
        print('     ✅ Exception: ${e.runtimeType}');
      }
    }

    // Test module disposal in various states
    print('\n   Testing disposal in various states...');

    final testModules = [
      Module(factory), // Fresh module
      Module(factory), // Module after failed load attempt
    ];

    // Try load on second module to put it in "attempted" state
    try {
      await testModules[1].load('/nonexistent');
    } catch (e) {
      // Expected
    }

    for (int i = 0; i < testModules.length; i++) {
      try {
        testModules[i].dispose();
        print('   Test module $i disposed successfully');
      } catch (e) {
        print('   Test module $i disposal failed: $e');
      }
    }

    module.dispose();
    print('✅ Advanced module features test completed');
  } catch (e) {
    print('❌ Advanced module features test failed: $e');
  }
}

/// Creates a mock module directory for testing.
Future<bool> createMockModule(String path) async {
  print('🔧 Creating Mock Module at: $path');

  try {
    final dir = Directory(path);
    await dir.create(recursive: true);

    // Create metadata file
    final metadataFile = File('$path/.fmod.json');
    const metadata = '''
{
  "name": "Test Module",
  "version": "1.0.0",
  "author": "Flow FFI Demo",
  "description": "A test module for demonstrating module loading",
  "library": "libtest_module",
  "nodes": [
    {
      "class": "TestInputNode",
      "category": "Test",
      "friendly_name": "Test Input"
    },
    {
      "class": "TestOutputNode", 
      "category": "Test",
      "friendly_name": "Test Output"
    }
  ]
}
''';

    await metadataFile.writeAsString(metadata);
    print('✅ Created metadata file');

    // Note: We can't create a real shared library here, but the metadata
    // file will allow us to test the module loading logic

    print('📋 Mock module created at: $path');
    print('   To make it functional, add a shared library:');
    print('   - Linux: libtest_module.so');
    print('   - macOS: libtest_module.dylib');
    print('   - Windows: test_module.dll');

    return true;
  } catch (e) {
    print('❌ Failed to create mock module: $e');
    return false;
  }
}

/// Runs module system stress test.
Future<void> runModuleStressTest({int moduleCount = 10}) async {
  print('🔥 Module System Stress Test');
  print('============================');

  final service = FlowService();

  try {
    await service.initialize(maxThreads: 4);
    final factory = service.factory!;

    print('Creating $moduleCount module instances...');

    final modules = <Module>[];
    final stopwatch = Stopwatch()..start();

    // Create many modules
    for (int i = 0; i < moduleCount; i++) {
      final module = Module(factory);
      modules.add(module);

      if ((i + 1) % 5 == 0) {
        print('  Created ${i + 1}/$moduleCount modules...');
      }
    }

    stopwatch.stop();
    print(
        '✅ Created $moduleCount modules in ${stopwatch.elapsedMilliseconds}ms');

    // Test operations on all modules
    print('Testing operations on all modules...');
    final opStopwatch = Stopwatch()..start();

    int successfulOps = 0;
    for (int i = 0; i < modules.length; i++) {
      try {
        final module = modules[i];

        // Basic property access
        final isValid = module.isValid;
        final isLoaded = module.isLoaded;
        final refCount = module.refCount;

        if (isValid && !isLoaded && refCount > 0) {
          successfulOps++;
        }
      } catch (e) {
        print('  Module $i operation failed: $e');
      }
    }

    opStopwatch.stop();
    print('✅ Tested operations in ${opStopwatch.elapsedMilliseconds}ms');
    print('  Successful operations: $successfulOps/$moduleCount');

    // Cleanup all modules
    print('Cleaning up modules...');
    final cleanupStopwatch = Stopwatch()..start();

    int cleanupSuccesses = 0;
    for (int i = 0; i < modules.length; i++) {
      try {
        modules[i].dispose();
        cleanupSuccesses++;
      } catch (e) {
        print('  Module $i cleanup failed: $e');
      }
    }

    cleanupStopwatch.stop();

    print('📊 Stress Test Results:');
    print('  Module creation: ${stopwatch.elapsedMilliseconds}ms');
    print('  Operations: ${opStopwatch.elapsedMilliseconds}ms');
    print('  Cleanup: ${cleanupStopwatch.elapsedMilliseconds}ms');
    print(
        '  Success rate: $cleanupSuccesses/$moduleCount (${(cleanupSuccesses / moduleCount * 100).toStringAsFixed(1)}%)');
  } catch (e) {
    print('❌ Module stress test failed: $e');
  } finally {
    await service.cleanup();
  }
}
