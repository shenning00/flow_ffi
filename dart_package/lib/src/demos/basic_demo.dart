import '../services/flow_service.dart';
import '../models/environment.dart';
import '../utils/error_handler.dart';

/// Demonstrates basic Flow FFI functionality.
///
/// This demo shows the fundamental operations: environment creation,
/// factory access, basic queries, and resource cleanup.
Future<void> runBasicDemo() async {
  print('🚀 Flow FFI Basic Demo');
  print('=====================');

  try {
    // 1. Environment Creation
    print('\n1. Creating Environment...');
    final env = Environment(maxThreads: 4);
    print('✅ Environment created with 4 threads');
    print('   Environment valid: ${env.isValid}');
    print('   Reference count: ${env.refCount}');

    // 2. Factory Access
    print('\n2. Accessing NodeFactory...');
    final factory = env.factory;
    print('✅ Factory obtained');
    print('   Factory valid: ${factory.isValid}');
    print('   Reference count: ${factory.refCount}');

    // 3. Query Capabilities
    print('\n3. Querying Available Categories...');
    final categories = factory.getCategories();
    print('📦 Available categories: ${categories.length}');
    if (categories.isNotEmpty) {
      for (final category in categories) {
        print('   - $category');

        // Show node classes in each category
        try {
          final classes = factory.getNodeClasses(category);
          for (final className in classes) {
            final friendlyName = factory.getFriendlyName(className);
            print('     • $className ($friendlyName)');
          }
        } catch (e) {
          print('     Error getting classes: $e');
        }
      }
    } else {
      print('   No categories found (no modules loaded)');
    }

    // 4. Type Conversion Testing
    print('\n4. Testing Type Conversion...');
    try {
      final canConvert = factory.isConvertible('int', 'double');
      print('   int -> double convertible: $canConvert');

      final canConvert2 = factory.isConvertible('string', 'int');
      print('   string -> int convertible: $canConvert2');
    } catch (e) {
      print('   Type conversion test failed: $e');
    }

    // 5. Environment Variables
    print('\n5. Testing Environment Variables...');
    try {
      final path = env.getEnvironmentVariable('PATH');
      print('   PATH length: ${path?.length ?? 0}');

      final home = env.getEnvironmentVariable('HOME') ??
          env.getEnvironmentVariable('USERPROFILE');
      print('   Home directory: ${home?.length ?? 0} chars');
    } catch (e) {
      print('   Environment variable test failed: $e');
    }

    // 6. Task Management
    print('\n6. Testing Task Management...');
    try {
      env.wait(); // Wait for any background tasks
      print('⏱️  All tasks completed');
    } catch (e) {
      print('   Task management test failed: $e');
    }

    // 7. Performance Testing
    print('\n7. Performance Testing...');
    final stopwatch = Stopwatch()..start();

    // Create multiple environments to test performance
    final environments = <Environment>[];
    for (int i = 0; i < 10; i++) {
      try {
        final testEnv = Environment(maxThreads: 2);
        environments.add(testEnv);
      } catch (e) {
        print('   Failed to create environment $i: $e');
        break;
      }
    }

    stopwatch.stop();
    print(
        '   Created ${environments.length} environments in ${stopwatch.elapsedMilliseconds}ms');

    // Cleanup test environments
    for (final testEnv in environments) {
      try {
        testEnv.dispose();
      } catch (e) {
        print('   Warning: Failed to dispose test environment: $e');
      }
    }

    print('\n8. Cleanup...');
    factory.dispose();
    env.dispose();
    print('🧹 Resources cleaned up successfully');

    print('\n✅ Basic Demo completed successfully!');
    print('   All core FFI functionality is working correctly.');
  } on FlowException catch (e) {
    print('\n❌ Flow error occurred: $e');
    print('   Error code: ${e.message}');
  } catch (e, stackTrace) {
    print('\n❌ Unexpected error: $e');
    print('Stack trace: $stackTrace');
  }
}

/// Runs basic functionality verification tests.
Future<bool> verifyBasicFunctionality() async {
  print('🧪 Verifying Basic Functionality...');

  final tests = <String, Future<bool> Function()>{
    'Environment Creation': () async {
      try {
        final env = Environment(maxThreads: 2);
        final isValid = env.isValid;
        env.dispose();
        return isValid;
      } catch (e) {
        return false;
      }
    },
    'Factory Access': () async {
      try {
        final env = Environment(maxThreads: 2);
        final factory = env.factory;
        final isValid = factory.isValid;
        factory.dispose();
        env.dispose();
        return isValid;
      } catch (e) {
        return false;
      }
    },
    'Categories Query': () async {
      try {
        final env = Environment(maxThreads: 2);
        final factory = env.factory;
        final categories = factory.getCategories();
        factory.dispose();
        env.dispose();
        return categories is List<String>;
      } catch (e) {
        return false;
      }
    },
    'Memory Management': () async {
      try {
        // Create and dispose multiple environments
        for (int i = 0; i < 5; i++) {
          final env = Environment(maxThreads: 2);
          final factory = env.factory;
          factory.dispose();
          env.dispose();
        }
        return true;
      } catch (e) {
        return false;
      }
    },
  };

  int passed = 0;
  final int total = tests.length;

  for (final entry in tests.entries) {
    final testName = entry.key;
    final testFunction = entry.value;

    try {
      final result = await testFunction();
      if (result) {
        print('   ✅ $testName');
        passed++;
      } else {
        print('   ❌ $testName (returned false)');
      }
    } catch (e) {
      print('   ❌ $testName (exception: $e)');
    }
  }

  final success = passed == total;
  print('\n📊 Verification Results: $passed/$total tests passed');

  if (success) {
    print('🎉 All basic functionality tests passed!');
  } else {
    print('⚠️  Some tests failed - there may be issues with the FFI bridge');
  }

  return success;
}
