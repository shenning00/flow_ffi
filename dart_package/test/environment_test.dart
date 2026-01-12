import 'package:test/test.dart';
import 'package:flow_ffi/src/models/environment.dart';
import 'package:flow_ffi/src/models/factory.dart';
import 'package:flow_ffi/src/utils/error_handler.dart';

void main() {
  group('Environment Tests', () {
    setUp(() {
      // Clear any pending errors before each test
      ErrorHandler.clearError();
    });

    test('create environment with default threads', () {
      final env = Environment();
      expect(env.isValid, isTrue);
      expect(env.refCount, equals(1));

      env.dispose();
    });

    test('create environment with custom thread count', () {
      final env = Environment(maxThreads: 8);
      expect(env.isValid, isTrue);
      expect(env.refCount, equals(1));

      env.dispose();
    });

    test('create environment with invalid thread count', () {
      expect(
        () => Environment(maxThreads: 0),
        throwsA(isA<InvalidArgumentException>()),
      );

      expect(
        () => Environment(maxThreads: -1),
        throwsA(isA<InvalidArgumentException>()),
      );
    });

    test('get factory from environment', () {
      final env = Environment(maxThreads: 2);
      final factory = env.factory;

      expect(factory.isValid, isTrue);
      expect(factory.refCount, equals(1));

      factory.dispose();
      env.dispose();
    });

    test('multiple factory access creates separate handles', () {
      final env = Environment(maxThreads: 2);
      final factory1 = env.factory;
      final factory2 = env.factory;

      // Should be different handle objects but reference same factory
      expect(identical(factory1, factory2), isFalse);
      expect(factory1.isValid, isTrue);
      expect(factory2.isValid, isTrue);

      factory1.dispose();
      factory2.dispose();
      env.dispose();
    });

    test('wait for tasks', () {
      final env = Environment(maxThreads: 2);

      // Should complete without error (no tasks running)
      expect(() => env.wait(), returnsNormally);

      env.dispose();
    });

    test('get environment variable', () {
      final env = Environment(maxThreads: 2);

      // Try to get PATH variable (should exist on most systems)
      final pathVar = env.getEnvironmentVariable('PATH');

      // PATH should exist on most systems
      if (pathVar != null) {
        expect(pathVar.isNotEmpty, isTrue);
      }

      env.dispose();
    });

    test('get non-existent environment variable', () {
      final env = Environment(maxThreads: 2);

      // Try to get a variable that definitely doesn't exist
      final result =
          env.getEnvironmentVariable('DEFINITELY_DOES_NOT_EXIST_12345');
      expect(result, isNull);

      env.dispose();
    });

    test('get environment variable with empty name', () {
      final env = Environment(maxThreads: 2);

      expect(
        () => env.getEnvironmentVariable(''),
        throwsA(isA<InvalidArgumentException>()),
      );

      env.dispose();
    });

    test('environment handle reference counting', () {
      final env = Environment(maxThreads: 2);
      expect(env.refCount, equals(1));

      env.retain();
      expect(env.refCount, equals(2));

      env.release();
      expect(env.refCount, equals(1));

      env.dispose();
    });

    test('environment toString', () {
      final env = Environment(maxThreads: 2);
      final str = env.toString();

      expect(str, contains('Environment'));
      expect(str, contains('refCount'));
      expect(str, contains('isValid'));

      env.dispose();
    });
  });
}
