import 'package:test/test.dart';
import 'package:flow_ffi/src/models/environment.dart';
import 'package:flow_ffi/src/models/factory.dart';
import 'package:flow_ffi/src/utils/error_handler.dart';

void main() {
  group('NodeFactory Tests', () {
    late Environment env;
    late NodeFactory factory;

    setUp(() {
      // Clear any pending errors before each test
      ErrorHandler.clearError();
      env = Environment(maxThreads: 2);
      factory = env.factory;
    });

    tearDown(() {
      factory.dispose();
      env.dispose();
    });

    test('factory handle properties', () {
      expect(factory.isValid, isTrue);
      expect(factory.refCount, equals(1));
    });

    test('get categories from empty factory', () {
      final categories = factory.getCategories();
      expect(categories, isEmpty);
    });

    test('get node classes for non-existent category', () {
      final classes = factory.getNodeClasses('NonExistentCategory');
      expect(classes, isEmpty);
    });

    test('get node classes with empty category name', () {
      expect(
        () => factory.getNodeClasses(''),
        throwsA(isA<InvalidArgumentException>()),
      );
    });

    test('get friendly name for non-existent class', () {
      // Should return something (either empty string or the class name itself)
      final name = factory.getFriendlyName('NonExistentClass');
      expect(name, isA<String>());
    });

    test('get friendly name with empty class name', () {
      expect(
        () => factory.getFriendlyName(''),
        throwsA(isA<InvalidArgumentException>()),
      );
    });

    test('check type convertibility', () {
      // Check basic type conversion (result depends on what's registered)
      final convertible = factory.isConvertible('int', 'double');
      expect(convertible, isA<bool>());

      // Same type should typically be convertible to itself
      final sameType = factory.isConvertible('int', 'int');
      expect(sameType, isA<bool>());
    });

    test('check convertibility with empty type names', () {
      expect(
        () => factory.isConvertible('', 'double'),
        throwsA(isA<InvalidArgumentException>()),
      );

      expect(
        () => factory.isConvertible('int', ''),
        throwsA(isA<InvalidArgumentException>()),
      );
    });

    test('create node with unregistered class', () {
      // Should throw since no classes are registered
      // It may throw NodeNotFoundException or UnknownFlowException depending on
      // the C++ error message, but it should throw a FlowException
      expect(
        () => factory.createNode('UnregisteredClass', env),
        throwsA(isA<FlowException>()),
      );
    });

    test('create node with empty class name', () {
      expect(
        () => factory.createNode('', env),
        throwsA(isA<InvalidArgumentException>()),
      );
    });

    test('get all node types from empty factory', () {
      final allTypes = factory.getAllNodeTypes();
      expect(allTypes, isEmpty);
    });

    test('factory reference counting', () {
      expect(factory.refCount, equals(1));

      factory.retain();
      expect(factory.refCount, equals(2));

      factory.release();
      expect(factory.refCount, equals(1));
    });

    test('factory toString', () {
      final str = factory.toString();

      expect(str, contains('NodeFactory'));
      expect(str, contains('refCount'));
      expect(str, contains('isValid'));
    });
  });

  group('NodeFactory Integration Tests', () {
    test('multiple factories from same environment', () {
      final env = Environment(maxThreads: 2);
      final factory1 = env.factory;
      final factory2 = env.factory;

      // Should be different objects
      expect(identical(factory1, factory2), isFalse);

      // But both should be valid
      expect(factory1.isValid, isTrue);
      expect(factory2.isValid, isTrue);

      // Operations should work on both
      expect(factory1.getCategories(), isEmpty);
      expect(factory2.getCategories(), isEmpty);

      factory1.dispose();
      factory2.dispose();
      env.dispose();
    });

    test('factory operations after environment disposal', () {
      final env = Environment(maxThreads: 2);
      final factory = env.factory;

      // Dispose environment first
      env.dispose();

      // Factory should still work due to reference counting
      // (though this depends on the underlying implementation)
      expect(factory.isValid, isTrue);

      factory.dispose();
    });
  });
}
