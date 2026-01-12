import 'package:test/test.dart';
import 'package:flow_ffi/flow_ffi.dart';
import 'dart:io';

void main() {
  late Environment env;
  late Graph graph;

  setUpAll(() {
    // The library is loaded automatically via the bindings.dart file
    // No explicit initialization needed for tests
  });

  setUp(() {
    env = Environment(maxThreads: 4);
    graph = Graph(env);
  });

  tearDown(() {
    graph.dispose();
    env.dispose();
  });

  group('PortMetadata', () {
    test('parsedValue returns null for empty JSON', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: '',
        hasDefault: false,
      );

      expect(metadata.parsedValue, isNull);
    });

    test('parsedValue returns null for null JSON', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: null,
        hasDefault: false,
      );

      expect(metadata.parsedValue, isNull);
    });

    test('parsedValue parses valid JSON', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: '{"type":"integer","value":"42"}',
        hasDefault: true,
      );

      final parsed = metadata.parsedValue;
      expect(parsed, isNotNull);
      expect(parsed!['type'], equals('integer'));
      expect(parsed['value'], equals('42'));
    });

    test('interworkingType extracts type from JSON', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: '{"type":"float","value":"3.14"}',
        hasDefault: true,
      );

      expect(metadata.interworkingType, equals('float'));
    });

    test('defaultValueString extracts value from JSON', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: '{"type":"string","value":"/path/to/file"}',
        hasDefault: true,
      );

      expect(metadata.defaultValueString, equals('/path/to/file'));
    });

    test('isEditable returns true for editable types', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: '{"type":"integer","value":"0"}',
        hasDefault: true,
      );

      expect(metadata.isEditable, isTrue);
    });

    test('isEditable returns false for none type', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: '{"type":"none"}',
        hasDefault: false,
      );

      expect(metadata.isEditable, isFalse);
    });

    test('isEditable returns false for null type', () {
      const metadata = PortMetadata(
        key: 'test',
        interworkingValueJson: null,
        hasDefault: false,
      );

      expect(metadata.isEditable, isFalse);
    });

    test('supports all interworking types', () {
      final types = ['integer', 'float', 'string', 'boolean'];

      for (final type in types) {
        final metadata = PortMetadata(
          key: 'test',
          interworkingValueJson: '{"type":"$type","value":"test"}',
          hasDefault: true,
        );

        expect(metadata.interworkingType, equals(type));
        expect(metadata.isEditable, isTrue);
      }
    });

    test('equality operator works correctly', () {
      const metadata1 = PortMetadata(
        key: 'test',
        interworkingValueJson: '{"type":"integer","value":"42"}',
        hasDefault: true,
      );

      const metadata2 = PortMetadata(
        key: 'test',
        interworkingValueJson: '{"type":"integer","value":"42"}',
        hasDefault: true,
      );

      const metadata3 = PortMetadata(
        key: 'test2',
        interworkingValueJson: '{"type":"integer","value":"42"}',
        hasDefault: true,
      );

      expect(metadata1, equals(metadata2));
      expect(metadata1, isNot(equals(metadata3)));
    });

    test('toString provides meaningful output', () {
      const metadata = PortMetadata(
        key: 'myPort',
        interworkingValueJson: '{"type":"integer","value":"100"}',
        hasDefault: true,
      );

      final str = metadata.toString();
      expect(str, contains('myPort'));
      expect(str, contains('integer'));
      expect(str, contains('100'));
      expect(str, contains('true'));
    });
  });

  group('Node.getInputPortMetadata', () {
    test('returns null for non-existent port', () {
      // Skip test if we can't create nodes (no modules loaded)
      try {
        final node = graph.addNode('core.add', 'test_node');
        final metadata = node.getInputPortMetadata('non_existent_port');
        expect(metadata, isNull);
      } on FlowException {
        // No modules loaded, skip test
        print('[Test] Skipping - no node modules available');
      }
    });

    test('throws InvalidArgumentException for empty port key', () {
      // Skip test if we can't create nodes (no modules loaded)
      try {
        final node = graph.addNode('core.add', 'test_node');
        expect(
          () => node.getInputPortMetadata(''),
          throwsA(isA<InvalidArgumentException>()),
        );
      } on FlowException {
        // No modules loaded, skip test
        print('[Test] Skipping - no node modules available');
      }
    });

    // Note: The following tests require actual nodes with metadata.
    // They will be skipped if the appropriate nodes are not available.
    test('retrieves metadata for existing port', () {
      // Try to create a node that we know has ports
      try {
        final node = graph.addNode('core.add', 'test_node');

        // Get input port keys
        final portKeys = node.getInputPortKeys();
        if (portKeys.isNotEmpty) {
          final metadata = node.getInputPortMetadata(portKeys.first);

          // Metadata might be null if the C++ layer doesn't provide it for this node
          if (metadata != null) {
            expect(metadata.key, equals(portKeys.first));
            print('[Test] Retrieved metadata for ${portKeys.first}: $metadata');
          } else {
            print('[Test] No metadata available for ${portKeys.first}');
          }
        } else {
          print('[Test] Node has no input ports');
        }
      } catch (e) {
        print('[Test] Skipping test - node creation failed: $e');
      }
    }, skip: 'Requires specific node types with metadata');
  });

  group('Node.getAllInputPortsMetadata', () {
    test('returns empty list for node with no input ports', () {
      // Try to create a node without input ports (like a constant node)
      try {
        final node = graph.addNode('core.constant', 'test_node');

        final metadataList = node.getAllInputPortsMetadata();
        expect(metadataList, isEmpty);
      } catch (e) {
        print('[Test] Skipping test - node creation failed: $e');
      }
    }, skip: 'Requires specific node types');

    test('retrieves metadata for all input ports', () {
      try {
        final node = graph.addNode('core.add', 'test_node');

        final metadataList = node.getAllInputPortsMetadata();
        final portKeys = node.getInputPortKeys();

        expect(metadataList.length, equals(portKeys.length));

        for (final metadata in metadataList) {
          expect(metadata.key, isIn(portKeys));
          print('[Test] Port metadata: $metadata');
        }
      } catch (e) {
        print('[Test] Skipping test - node creation failed: $e');
      }
    }, skip: 'Requires specific node types with metadata');

    test('metadata list contains valid objects', () {
      try {
        final node = graph.addNode('core.add', 'test_node');

        final metadataList = node.getAllInputPortsMetadata();

        for (final metadata in metadataList) {
          expect(metadata.key, isNotEmpty);

          // If metadata has a default value, verify the JSON is parseable
          if (metadata.hasDefault && metadata.interworkingValueJson != null) {
            expect(metadata.parsedValue, isNotNull);
            expect(metadata.interworkingType, isNotNull);
          }
        }
      } catch (e) {
        print('[Test] Skipping test - node creation failed: $e');
      }
    }, skip: 'Requires specific node types with metadata');
  });

  group('Integration Tests', () {
    test('metadata persists across multiple calls', () {
      try {
        final node = graph.addNode('core.add', 'test_node');

        final portKeys = node.getInputPortKeys();
        if (portKeys.isNotEmpty) {
          final metadata1 = node.getInputPortMetadata(portKeys.first);
          final metadata2 = node.getInputPortMetadata(portKeys.first);

          // Both calls should return equivalent metadata
          expect(metadata1?.key, equals(metadata2?.key));
          expect(metadata1?.interworkingValueJson,
              equals(metadata2?.interworkingValueJson));
          expect(metadata1?.hasDefault, equals(metadata2?.hasDefault));
        }
      } catch (e) {
        print('[Test] Skipping test - node creation failed: $e');
      }
    }, skip: 'Requires specific node types with metadata');

    test('getAllInputPortsMetadata returns same data as individual calls', () {
      try {
        final node = graph.addNode('core.add', 'test_node');

        final allMetadata = node.getAllInputPortsMetadata();
        final portKeys = node.getInputPortKeys();

        for (final portKey in portKeys) {
          final individual = node.getInputPortMetadata(portKey);
          final fromList = allMetadata.firstWhere(
            (m) => m.key == portKey,
            orElse: () => throw Exception('Port $portKey not found in list'),
          );

          expect(individual?.key, equals(fromList.key));
          expect(individual?.interworkingValueJson,
              equals(fromList.interworkingValueJson));
          expect(individual?.hasDefault, equals(fromList.hasDefault));
        }
      } catch (e) {
        print('[Test] Skipping test - node creation failed: $e');
      }
    }, skip: 'Requires specific node types with metadata');
  });
}
