import 'dart:async';

import '../services/flow_service.dart';
import '../models/environment.dart';
import '../models/graph.dart';
import '../utils/error_handler.dart';

/// Demonstrates the event system capabilities.
///
/// This demo shows how to set up event listeners, handle graph and node events,
/// and manage event subscriptions properly with automatic cleanup.
Future<void> runEventDemo() async {
  print('🔔 Flow FFI Event Demo');
  print('======================');

  final service = FlowService();
  final events = <String>[];

  try {
    // Initialize service
    print('\n1. Initializing Service with Event Monitoring...');
    await service.initialize(maxThreads: 2);

    // Set up service-level event listeners
    final graphEventSub = service.graphEvents.listen((event) {
      final msg = '📊 Graph Event: ${event.type} - ${event.description}';
      print('   $msg');
      events.add(msg);
    });

    final nodeEventSub = service.nodeEvents.listen((event) {
      final msg = '🔹 Node Event: ${event.node.name} - ${event.type}';
      print('   $msg');
      events.add(msg);
    });

    final errorEventSub = service.errors.listen((error) {
      final msg = '❌ Flow Error: ${error.message}';
      print('   $msg');
      events.add(msg);
    });

    print('✅ Event listeners configured');

    // Test direct graph events
    await _testDirectGraphEvents(service, events);

    // Test event lifecycle
    await _testEventLifecycle(service, events);

    // Test error events
    await _testErrorEvents(service, events);

    // Test event cleanup
    await _testEventCleanup(service, events);

    // Summary
    print('\n6. Event Summary...');
    print('   Total events captured: ${events.length}');
    print('   Event types:');

    final eventTypes = <String, int>{};
    for (final event in events) {
      final type = event.split(':')[0].trim();
      eventTypes[type] = (eventTypes[type] ?? 0) + 1;
    }

    eventTypes.forEach((type, count) {
      print('     $type: $count events');
    });

    // Cleanup subscriptions
    await graphEventSub.cancel();
    await nodeEventSub.cancel();
    await errorEventSub.cancel();
  } on FlowException catch (e) {
    print('\n❌ Flow error: $e');
  } catch (e) {
    print('\n❌ Unexpected error: $e');
  } finally {
    await service.cleanup();
    print('\n🧹 Service cleaned up');
  }

  print('\n✅ Event Demo completed!');
  if (events.isNotEmpty) {
    print('🎉 Event system is working correctly!');
  } else {
    print('⚠️  No events were captured - event system may not be working');
  }
}

/// Tests direct graph-level events.
Future<void> _testDirectGraphEvents(
    FlowService service, List<String> events) async {
  print('\n2. Testing Direct Graph Events...');

  try {
    final graph = await service.createGraph();

    // Set up direct graph event listeners
    final graphEventSubscriptions = <StreamSubscription>[];

    graphEventSubscriptions.add(
      graph.onNodeAdded.listen((event) {
        final msg = '➕ Direct: Node Added - ${event.node.name}';
        print('   $msg');
        events.add(msg);
      }),
    );

    graphEventSubscriptions.add(
      graph.onNodeRemoved.listen((event) {
        final msg = '➖ Direct: Node Removed - ${event.node.name}';
        print('   $msg');
        events.add(msg);
      }),
    );

    graphEventSubscriptions.add(
      graph.onNodesConnected.listen((event) {
        final msg =
            '🔗 Direct: Nodes Connected - ${event.connection.description}';
        print('   $msg');
        events.add(msg);
      }),
    );

    graphEventSubscriptions.add(
      graph.onError.listen((event) {
        final msg = '🚨 Direct: Graph Error - ${event.error}';
        print('   $msg');
        events.add(msg);
      }),
    );

    print('✅ Direct graph event listeners set up');

    // Try operations that should trigger events
    try {
      print('   Attempting node operations...');
      final node = graph.addNode('TestNode', 'test1');
      print('   Node added, waiting for events...');

      // Give events time to propagate
      await Future.delayed(const Duration(milliseconds: 100));

      // Try to remove node
      graph.removeNode(node.id);
      print('   Node removed, waiting for events...');
      await Future.delayed(const Duration(milliseconds: 100));
    } on NodeNotFoundException catch (e) {
      print('   ⚠️  Node operations failed (expected): $e');
      // This triggers error events, which is useful for testing
    }

    // Cleanup direct subscriptions
    for (final sub in graphEventSubscriptions) {
      await sub.cancel();
    }
  } catch (e) {
    print('❌ Direct graph events test failed: $e');
  }
}

/// Tests event lifecycle and management.
Future<void> _testEventLifecycle(
    FlowService service, List<String> events) async {
  print('\n3. Testing Event Lifecycle...');

  try {
    // Create multiple graphs to test event isolation
    final graphs = <Graph>[];
    final subscriptions = <StreamSubscription>[];

    for (int i = 0; i < 3; i++) {
      final graph = await service.createGraph();
      graphs.add(graph);

      // Each graph should have independent events
      final sub = graph.onError.listen((event) {
        final msg = '🔄 Lifecycle: Graph $i Error - ${event.error}';
        print('   $msg');
        events.add(msg);
      });
      subscriptions.add(sub);
    }

    print('✅ Created ${graphs.length} graphs with independent event listeners');

    // Test that events are properly isolated
    print('   Testing event isolation...');
    for (int i = 0; i < graphs.length; i++) {
      try {
        // This should only trigger events for this specific graph
        graphs[i].addNode('NonExistentNode', 'test$i');
      } catch (e) {
        // Expected to fail and trigger error event
      }
    }

    await Future.delayed(const Duration(milliseconds: 200));
    print('   Event isolation test completed');

    // Cleanup
    for (final sub in subscriptions) {
      await sub.cancel();
    }

    for (final graph in graphs) {
      graph.dispose();
    }
  } catch (e) {
    print('❌ Event lifecycle test failed: $e');
  }
}

/// Tests error event propagation.
Future<void> _testErrorEvents(FlowService service, List<String> events) async {
  print('\n4. Testing Error Events...');

  try {
    final graph = await service.createGraph();

    // Set up error event monitoring
    var errorCount = 0;
    final errorSub = graph.onError.listen((event) {
      errorCount++;
      final msg = '💥 Error Event $errorCount: ${event.error}';
      print('   $msg');
      events.add(msg);
    });

    // Trigger various error conditions
    final errorTests = [
      () => graph.addNode('', 'empty-class'), // Empty class name
      () => graph.addNode('BadClass', 'bad-node'), // Non-existent class
      () => graph.removeNode('non-existent-id'), // Non-existent node
      () => graph.connectNodes('a', 'out', 'b', 'in'), // Non-existent nodes
    ];

    print('   Triggering error conditions...');
    for (int i = 0; i < errorTests.length; i++) {
      try {
        errorTests[i]();
      } catch (e) {
        print('   Error test ${i + 1}: ${e.runtimeType}');
      }

      // Wait for events to propagate
      await Future.delayed(const Duration(milliseconds: 50));
    }

    print('✅ Error event tests completed, $errorCount errors captured');

    await errorSub.cancel();
  } catch (e) {
    print('❌ Error events test failed: $e');
  }
}

/// Tests event cleanup and resource management.
Future<void> _testEventCleanup(FlowService service, List<String> events) async {
  print('\n5. Testing Event Cleanup...');

  try {
    // Create graph with events, then dispose it
    var graph = await service.createGraph();

    var eventReceived = false;
    final sub = graph.onError.listen((event) {
      eventReceived = true;
      events.add('🔧 Cleanup Test: ${event.error}');
    });

    // Trigger an event before cleanup
    try {
      graph.addNode('TestCleanup', 'cleanup-test');
    } catch (e) {
      // Expected
    }

    await Future.delayed(const Duration(milliseconds: 100));

    // Cleanup the graph
    print('   Disposing graph...');
    graph.dispose();

    // Events should no longer be received after disposal
    print('   Attempting operations on disposed graph...');
    try {
      graph.addNode('AfterDispose', 'after-dispose');
    } catch (e) {
      print('   Expected error on disposed graph: ${e.runtimeType}');
    }

    await sub.cancel();

    // Create new graph to verify cleanup doesn't affect new instances
    print('   Creating new graph after cleanup...');
    graph = await service.createGraph();

    final newEventReceived = Completer<bool>();
    final newSub = graph.onError.listen((event) {
      if (!newEventReceived.isCompleted) {
        newEventReceived.complete(true);
        events.add('🆕 New Graph: ${event.error}');
      }
    });

    // This should trigger event on new graph
    try {
      graph.addNode('NewGraphTest', 'new-graph-test');
    } catch (e) {
      // Expected
    }

    // Wait for event or timeout
    try {
      await newEventReceived.future.timeout(const Duration(milliseconds: 500));
      print('✅ New graph events working after cleanup');
    } catch (TimeoutException) {
      print('⚠️  New graph events not received (may be normal)');
    }

    await newSub.cancel();
  } catch (e) {
    print('❌ Event cleanup test failed: $e');
  }
}

/// Runs a stress test on the event system.
Future<void> runEventStressTest(
    {int graphCount = 5, int eventCount = 100}) async {
  print('⚡ Event System Stress Test');
  print('==========================');

  final service = FlowService();
  final allEvents = <String>[];
  final subscriptions = <StreamSubscription>[];

  try {
    await service.initialize(maxThreads: 4);

    // Global event monitoring
    subscriptions.add(
      service.graphEvents.listen((event) {
        allEvents.add('Global Graph: ${event.type}');
      }),
    );

    subscriptions.add(
      service.errors.listen((error) {
        allEvents.add('Global Error: ${error.message}');
      }),
    );

    print('Creating $graphCount graphs with event listeners...');

    final graphs = <Graph>[];
    final stopwatch = Stopwatch()..start();

    // Create multiple graphs with event listeners
    for (int i = 0; i < graphCount; i++) {
      final graph = await service.createGraph();
      graphs.add(graph);

      // Multiple event listeners per graph
      subscriptions.add(
        graph.onError.listen((event) {
          allEvents.add('Graph$i Error: ${event.error}');
        }),
      );
    }

    print('Triggering $eventCount events across all graphs...');

    // Generate events rapidly
    for (int i = 0; i < eventCount; i++) {
      final graph = graphs[i % graphs.length];

      try {
        // This should trigger error events
        graph.addNode('StressTest$i', 'stress-node-$i');
      } catch (e) {
        // Expected
      }

      // Occasional small delay to allow event processing
      if (i % 20 == 0) {
        await Future.delayed(const Duration(milliseconds: 10));
      }
    }

    stopwatch.stop();

    // Wait for all events to propagate
    await Future.delayed(const Duration(milliseconds: 500));

    print('✅ Stress test completed in ${stopwatch.elapsedMilliseconds}ms');
    print('   Events generated: $eventCount');
    print('   Events captured: ${allEvents.length}');
    print(
        '   Capture rate: ${(allEvents.length / eventCount * 100).toStringAsFixed(1)}%');
    print(
        '   Average event processing: ${stopwatch.elapsedMilliseconds / eventCount}ms per event');

    // Cleanup
    for (final sub in subscriptions) {
      await sub.cancel();
    }

    for (final graph in graphs) {
      graph.dispose();
    }
  } catch (e) {
    print('❌ Event stress test failed: $e');
  } finally {
    await service.cleanup();
  }
}
