import 'dart:io';

import 'basic_demo.dart';
import 'graph_demo.dart';
import 'event_demo.dart';
import 'module_demo.dart';

/// Main demonstration application showcasing all Flow FFI capabilities.
///
/// This runs through all demonstration scenarios in sequence,
/// providing a comprehensive showcase of the FFI bridge functionality.
Future<void> main(List<String> args) async {
  print('🎯 Flow FFI Complete Demonstration');
  print('==================================');
  print('This demo showcases all capabilities of the Flow FFI bridge.\n');

  // Parse command line arguments
  final options = _parseArgs(args);

  if (options['help'] == true) {
    _showHelp();
    return;
  }

  print('Available demonstrations:');
  print('1. Basic Demo - Environment, Factory, and Core Operations');
  print('2. Graph Demo - Graph Creation, Nodes, and Connections');
  print('3. Event Demo - Event System and Stream Handling');
  print('4. Module Demo - Dynamic Module Loading');
  print('5. Interactive CLI - Real-time Interactive Interface');
  print('6. All Demos - Run all demonstrations in sequence');
  print('7. Stress Tests - Performance and Reliability Testing');

  final selection = options['demo'] ?? await _promptSelection();

  try {
    switch (selection) {
      case '1':
      case 'basic':
        await runBasicDemo();
        break;
      case '2':
      case 'graph':
        await runGraphDemo();
        break;
      case '3':
      case 'event':
        await runEventDemo();
        break;
      case '4':
      case 'module':
        await runModuleDemo();
        break;
      case '5':
      case 'interactive':
        await runInteractiveDemo();
        break;
      case '6':
      case 'all':
        await runAllDemos();
        break;
      case '7':
      case 'stress':
        await runStressTests();
        break;
      default:
        print('Invalid selection: $selection');
        exit(1);
    }

    print('\n🎉 Demonstration completed successfully!');
    print('The Flow FFI bridge is working correctly.');
  } catch (e, stackTrace) {
    print('\n💥 Demo failed with error: $e');
    if (options['verbose'] == true) {
      print('Stack trace: $stackTrace');
    }
    exit(1);
  }
}

/// Runs all demonstrations in sequence.
Future<void> runAllDemos() async {
  print('\n🎪 Running All Demonstrations');
  print('=============================\n');

  final demos = [
    ('Basic Demo', runBasicDemo),
    ('Graph Demo', runGraphDemo),
    ('Event Demo', runEventDemo),
    ('Module Demo', runModuleDemo),
  ];

  final stopwatch = Stopwatch()..start();

  for (int i = 0; i < demos.length; i++) {
    final name = demos[i].$1;
    final function = demos[i].$2;

    print('\n🎬 Starting $name (${i + 1}/${demos.length})');
    print('-' * (name.length + 20));

    try {
      await function();
      print('✅ $name completed successfully');
    } catch (e) {
      print('❌ $name failed: $e');
      print('Continuing with remaining demos...');
    }

    if (i < demos.length - 1) {
      print('\nPress Enter to continue to next demo...');
      stdin.readLineSync();
    }
  }

  stopwatch.stop();

  print('\n🏁 All Demonstrations Complete');
  print('==============================');
  print('Total time: ${stopwatch.elapsedMilliseconds / 1000}s');
  print('Demonstrations run: ${demos.length}');
}

/// Runs stress tests for performance and reliability.
Future<void> runStressTests() async {
  print('\n🔥 Flow FFI Stress Tests');
  print('========================\n');

  print('Select stress test:');
  print('1. Graph Stress Test');
  print('2. Event Stress Test');
  print('3. Module Stress Test');
  print('4. All Stress Tests');

  stdout.write('Selection (1-4): ');
  final selection = stdin.readLineSync()?.trim() ?? '1';

  try {
    switch (selection) {
      case '1':
        await runGraphStressTest();
        break;
      case '2':
        await runEventStressTest();
        break;
      case '3':
        await runModuleStressTest();
        break;
      case '4':
        print('Running all stress tests...\n');
        await runGraphStressTest();
        print('\n');
        await runEventStressTest();
        print('\n');
        await runModuleStressTest();
        break;
      default:
        print('Invalid selection, running graph stress test...');
        await runGraphStressTest();
        break;
    }
  } catch (e) {
    print('❌ Stress test failed: $e');
  }
}

/// Interactive demonstration with user input.
Future<void> runInteractiveDemo() async {
  print('\n🎮 Flow FFI Interactive Demo');
  print('============================');
  print('Commands: basic, graph, event, module, help, quit\n');

  while (true) {
    stdout.write('flow_demo> ');
    final input = stdin.readLineSync()?.trim().toLowerCase();

    if (input == null || input.isEmpty) {
      continue;
    }

    try {
      switch (input) {
        case 'help':
        case '?':
          _showInteractiveHelp();
          break;
        case 'basic':
          await runBasicDemo();
          break;
        case 'graph':
          await runGraphDemo();
          break;
        case 'event':
          await runEventDemo();
          break;
        case 'module':
          await runModuleDemo();
          break;
        case 'verify':
          final success = await verifyBasicFunctionality();
          if (success) {
            print('🎉 All functionality verified!');
          } else {
            print('⚠️  Some issues detected.');
          }
          break;
        case 'stress':
          await runStressTests();
          break;
        case 'clear':
          // Clear screen
          print('\x1B[2J\x1B[0;0H');
          print('🎮 Flow FFI Interactive Demo');
          print('============================');
          break;
        case 'quit':
        case 'exit':
          print('👋 Goodbye!');
          return;
        default:
          print('Unknown command: $input');
          print('Type "help" for available commands.');
          break;
      }
    } catch (e) {
      print('❌ Command failed: $e');
    }

    print(''); // Add spacing
  }
}

/// Parses command line arguments.
Map<String, dynamic> _parseArgs(List<String> args) {
  final options = <String, dynamic>{};

  for (int i = 0; i < args.length; i++) {
    final arg = args[i];

    if (arg.startsWith('--')) {
      final key = arg.substring(2);

      if (key == 'help') {
        options['help'] = true;
      } else if (key == 'verbose') {
        options['verbose'] = true;
      } else if (key == 'demo' && i + 1 < args.length) {
        options['demo'] = args[i + 1];
        i++; // Skip next arg
      }
    } else if (arg.startsWith('-')) {
      final key = arg.substring(1);

      if (key == 'h') {
        options['help'] = true;
      } else if (key == 'v') {
        options['verbose'] = true;
      }
    } else if (!options.containsKey('demo')) {
      // First non-flag argument is the demo selection
      options['demo'] = arg;
    }
  }

  return options;
}

/// Prompts user for demo selection.
Future<String> _promptSelection() async {
  while (true) {
    stdout.write('\nSelect demonstration (1-7): ');
    final input = stdin.readLineSync()?.trim();

    if (input != null && input.isNotEmpty) {
      return input;
    }

    print('Please enter a valid selection.');
  }
}

/// Shows help information.
void _showHelp() {
  print('Flow FFI Demonstration Application');
  print('');
  print('Usage: dart run main_demo.dart [options] [demo]');
  print('');
  print('Arguments:');
  print('  demo                 Demo to run (1-7, or name)');
  print('');
  print('Options:');
  print('  --help, -h          Show this help message');
  print('  --verbose, -v       Show detailed error information');
  print('  --demo <name>       Specify demo to run');
  print('');
  print('Demo names:');
  print('  1, basic            Basic functionality demo');
  print('  2, graph            Graph operations demo');
  print('  3, event            Event system demo');
  print('  4, module           Module loading demo');
  print('  5, interactive      Interactive CLI demo');
  print('  6, all              Run all demos');
  print('  7, stress           Stress tests');
  print('');
  print('Examples:');
  print('  dart run main_demo.dart basic');
  print('  dart run main_demo.dart --demo graph');
  print('  dart run main_demo.dart --verbose all');
}

/// Shows interactive help.
void _showInteractiveHelp() {
  print('Interactive Demo Commands:');
  print('  basic     - Run basic functionality demo');
  print('  graph     - Run graph operations demo');
  print('  event     - Run event system demo');
  print('  module    - Run module loading demo');
  print('  verify    - Verify basic functionality');
  print('  stress    - Run stress tests');
  print('  clear     - Clear screen');
  print('  help, ?   - Show this help');
  print('  quit, exit - Exit interactive mode');
}
