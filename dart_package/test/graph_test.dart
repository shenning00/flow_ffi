import 'package:test/test.dart';
import 'package:flow_ffi/flow_ffi.dart';

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
}
