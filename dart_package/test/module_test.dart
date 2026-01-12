import 'package:test/test.dart';
import 'package:flow_ffi/flow_ffi.dart';

void main() {
  group('Module Tests', () {
    late Environment env;
    late NodeFactory factory;

    setUp(() {
      env = Environment(maxThreads: 2);
      factory = env.factory;
    });

    tearDown(() {
      factory.dispose();
      env.dispose();
    });

    test('Module creation and basic properties', () {
      final module = Module(factory);

      expect(module.isValid, isTrue);
      expect(module.refCount, equals(1));
      expect(module.isLoaded, isFalse);
      expect(module.metadata, isNull);

      module.dispose();
      expect(module.isValid, isFalse);
    });

    test('Module load with invalid path should throw', () {
      final module = Module(factory);

      expect(() => module.load(''), throwsA(isA<ModuleException>()));
      expect(() => module.load('/nonexistent/path'),
          throwsA(isA<ModuleException>()));

      module.dispose();
    });

    test('Module operations on invalid handle should throw', () {
      final module = Module(factory);
      module.dispose();

      expect(() => module.load('/some/path'), throwsA(isA<ModuleException>()));
      expect(() => module.unload(), throwsA(isA<ModuleException>()));
      expect(() => module.registerNodes(), throwsA(isA<ModuleException>()));
      expect(() => module.unregisterNodes(), throwsA(isA<ModuleException>()));
    });

    test('Module node operations on unloaded module should throw', () {
      final module = Module(factory);

      expect(() => module.registerNodes(), throwsA(isA<ModuleException>()));
      expect(() => module.unregisterNodes(), throwsA(isA<ModuleException>()));

      module.dispose();
    });

    test('Module reference counting', () {
      final module = Module(factory);
      final initialRefCount = module.refCount;

      module.retain();
      expect(module.refCount, equals(initialRefCount + 1));

      module.release();
      expect(module.refCount, equals(initialRefCount));

      module.dispose();
    });

    test('Module handle validation', () {
      final module = Module(factory);

      expect(module.isValid, isTrue);
      expect(module.isValid, isTrue);

      module.dispose();

      expect(module.isValid, isFalse);
      expect(module.isValid, isFalse);
    });

    test('Module toString', () {
      final module = Module(factory);

      final str = module.toString();
      expect(str, contains('Module('));
      expect(str, contains('loaded: false'));
      expect(str, contains('refCount: 1'));
      expect(str, contains('isValid: true'));

      module.dispose();
    });

    test('ModuleMetaData creation and properties', () {
      const metadata = ModuleMetaData(
        name: 'TestModule',
        version: '1.0.0',
        author: 'Test Author',
        description: 'A test module for unit testing',
      );

      expect(metadata.name, equals('TestModule'));
      expect(metadata.version, equals('1.0.0'));
      expect(metadata.author, equals('Test Author'));
      expect(metadata.description, equals('A test module for unit testing'));

      final str = metadata.toString();
      expect(str, contains('TestModule'));
      expect(str, contains('1.0.0'));
      expect(str, contains('Test Author'));
    });

    test('ModuleException creation and properties', () {
      const exception1 = ModuleException('Test error message');
      expect(exception1.message, equals('Test error message'));
      expect(exception1.errorCode, isNull);
      expect(
          exception1.toString(), equals('ModuleException: Test error message'));

      const exception2 = ModuleException('Test error with code', 42);
      expect(exception2.message, equals('Test error with code'));
      expect(exception2.errorCode, equals(42));
      expect(exception2.toString(),
          equals('ModuleException: Test error with code (code: 42)'));
    });

    test('Multiple module instances', () {
      final module1 = Module(factory);
      final module2 = Module(factory);

      expect(module1.isValid, isTrue);
      expect(module2.isValid, isTrue);
      expect(module1.handle, isNot(equals(module2.handle)));

      module1.dispose();
      expect(module1.isValid, isFalse);
      expect(module2.isValid, isTrue);

      module2.dispose();
      expect(module2.isValid, isFalse);
    });

    test('Module unload when not loaded should succeed', () async {
      final module = Module(factory);

      expect(module.isLoaded, isFalse);
      final result = await module.unload();
      expect(result, isTrue);

      module.dispose();
    });
  });
}
