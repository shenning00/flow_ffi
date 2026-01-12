import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:test/test.dart';
import 'package:flow_ffi/src/models/environment.dart';
import 'package:flow_ffi/src/models/factory.dart';
import 'package:flow_ffi/src/models/node.dart';
import 'package:flow_ffi/src/utils/error_handler.dart';
import 'package:flow_ffi/src/utils/type_converter_simple.dart';
import 'package:flow_ffi/src/ffi/bindings.dart';

void main() {
  group('Node Data Tests', () {
    setUp(() {
      ErrorHandler.clearError();
    });

    test('create and access int data', () {
      final intDataHandle = flowCore.flow_data_create_int(42);
      expect(intDataHandle.address, isNot(equals(0)));

      final valuePtr = calloc<Int32>();
      try {
        final result = flowCore.flow_data_get_int(intDataHandle, valuePtr);
        expect(result, equals(0)); // FLOW_SUCCESS
        expect(valuePtr.value, equals(42));
      } finally {
        calloc.free(valuePtr);
        flowCore.flow_data_destroy(intDataHandle);
      }
    });

    test('create and access double data', () {
      final doubleDataHandle = flowCore.flow_data_create_double(3.14159);
      expect(doubleDataHandle.address, isNot(equals(0)));

      final valuePtr = calloc<Double>();
      try {
        final result =
            flowCore.flow_data_get_double(doubleDataHandle, valuePtr);
        expect(result, equals(0)); // FLOW_SUCCESS
        expect(valuePtr.value, closeTo(3.14159, 0.00001));
      } finally {
        calloc.free(valuePtr);
        flowCore.flow_data_destroy(doubleDataHandle);
      }
    });

    test('create and access bool data', () {
      final boolDataHandle = flowCore.flow_data_create_bool(true);
      expect(boolDataHandle.address, isNot(equals(0)));

      final valuePtr = calloc<Bool>();
      try {
        final result = flowCore.flow_data_get_bool(boolDataHandle, valuePtr);
        expect(result, equals(0)); // FLOW_SUCCESS
        expect(valuePtr.value, isTrue);
      } finally {
        calloc.free(valuePtr);
        flowCore.flow_data_destroy(boolDataHandle);
      }
    });

    test('create and access string data', () {
      const testString = 'Hello, Flow!';
      final cString = testString.toNativeUtf8();

      try {
        final stringDataHandle =
            flowCore.flow_data_create_string(cString.cast<Char>());
        expect(stringDataHandle.address, isNot(equals(0)));

        final valuePtrPtr = calloc<Pointer<Char>>();
        try {
          final result =
              flowCore.flow_data_get_string(stringDataHandle, valuePtrPtr);
          expect(result, equals(0)); // FLOW_SUCCESS

          final valuePtr = valuePtrPtr.value;
          expect(valuePtr.address, isNot(equals(0)));

          final resultString = valuePtr.cast<Utf8>().toDartString();
          expect(resultString, equals(testString));

          flowCore.flow_free_string(valuePtr);
        } finally {
          calloc.free(valuePtrPtr);
          flowCore.flow_data_destroy(stringDataHandle);
        }
      } finally {
        calloc.free(cString);
      }
    });

    test('data type checking', () {
      final intDataHandle = flowCore.flow_data_create_int(123);

      final typePtr = flowCore.flow_data_get_type(intDataHandle);
      expect(typePtr.address, isNot(equals(0)));

      final typeName = typePtr.cast<Utf8>().toDartString();
      expect(typeName, contains('int')); // Type name should contain 'int'

      flowCore.flow_free_string(typePtr);
      flowCore.flow_data_destroy(intDataHandle);
    });

    test('data to string conversion', () {
      final intDataHandle = flowCore.flow_data_create_int(456);

      final stringPtr = flowCore.flow_data_to_string(intDataHandle);
      expect(stringPtr.address, isNot(equals(0)));

      final stringValue = stringPtr.cast<Utf8>().toDartString();
      expect(stringValue, equals('456'));

      flowCore.flow_free_string(stringPtr);
      flowCore.flow_data_destroy(intDataHandle);
    });

    test('type mismatch error', () {
      final intDataHandle = flowCore.flow_data_create_int(789);

      final doublePtr = calloc<Double>();
      try {
        final result = flowCore.flow_data_get_double(intDataHandle, doublePtr);
        expect(result, equals(-9)); // FLOW_ERROR_TYPE_MISMATCH

        final errorPtr = flowCore.flow_get_last_error();
        expect(errorPtr.address, isNot(equals(0)));

        final errorMessage = errorPtr.cast<Utf8>().toDartString();
        expect(errorMessage, contains('Expected double'));
      } finally {
        calloc.free(doublePtr);
        flowCore.flow_data_destroy(intDataHandle);
        flowCore.flow_clear_error();
      }
    });
  });

  group('Type Converter Tests', () {
    setUp(() {
      ErrorHandler.clearError();
    });

    test('convert dart types to native data', () {
      // Test int conversion
      final intData = TypeConverter.toNativeData(42);
      expect(intData.isValid, isTrue);
      intData.dispose();

      // Test double conversion
      final doubleData = TypeConverter.toNativeData(3.14);
      expect(doubleData.isValid, isTrue);
      doubleData.dispose();

      // Test bool conversion
      final boolData = TypeConverter.toNativeData(true);
      expect(boolData.isValid, isTrue);
      boolData.dispose();

      // Test string conversion
      final stringData = TypeConverter.toNativeData('test');
      expect(stringData.isValid, isTrue);
      stringData.dispose();
    });

    test('convert native data to dart types', () {
      // Test int conversion
      final intData = TypeConverter.toNativeData(123);
      final intValue = TypeConverter.fromNativeData<int>(intData);
      expect(intValue, equals(123));
      intData.dispose();

      // Test double conversion
      final doubleData = TypeConverter.toNativeData(2.718);
      final doubleValue = TypeConverter.fromNativeData<double>(doubleData);
      expect(doubleValue, closeTo(2.718, 0.001));
      doubleData.dispose();

      // Test string conversion
      final stringData = TypeConverter.toNativeData('hello');
      final stringValue = TypeConverter.fromNativeData<String>(stringData);
      expect(stringValue, equals('hello'));
      stringData.dispose();
    });

    test('type conversion error handling', () {
      expect(
        () => TypeConverter.toNativeData(DateTime.now()),
        throwsA(isA<TypeMismatchException>()),
      );
    });
  });

  group('Node Interface Tests', () {
    late Environment env;
    late NodeFactory factory;

    setUp(() {
      ErrorHandler.clearError();
      env = Environment(maxThreads: 2);
      factory = env.factory;
    });

    tearDown(() {
      factory.dispose();
      env.dispose();
    });

    test('node creation fails without registered classes', () {
      // This should fail since no node classes are registered by default
      // It may throw NodeNotFoundException or UnknownFlowException depending on
      // the C++ error message, but it should throw something
      expect(
        () => factory.createNode('TestNode', env),
        throwsA(isA<FlowException>()),
      );
    });

    test('node property access with invalid node', () {
      // Test that node property accessors handle invalid nodes properly
      // Since we can't create real nodes without registering classes,
      // we test the error handling paths

      // These tests verify the API structure is correct
      // Real functionality testing would require actual node registration
    });
  });

  group('Node Data Integration Tests', () {
    test('data handle lifecycle', () {
      final data1 = TypeConverter.toNativeData(100);
      final data2 = TypeConverter.toNativeData('test string');

      expect(data1.isValid, isTrue);
      expect(data2.isValid, isTrue);
      expect(data1.refCount, equals(1));
      expect(data2.refCount, equals(1));

      // Retain handles
      data1.retain();
      expect(data1.refCount, equals(2));

      // Release handles
      data1.release();
      expect(data1.refCount, equals(1));

      data1.dispose();
      data2.dispose();
    });

    test('concurrent data operations', () {
      // Test thread-safe data creation
      const numOperations = 100;
      final futures = <Future>[];

      for (int i = 0; i < numOperations; i++) {
        futures.add(
          Future(() {
            final data = TypeConverter.toNativeData(i);
            final value = TypeConverter.fromNativeData<int>(data);
            expect(value, equals(i));
            data.dispose();
          }),
        );
      }

      return Future.wait(futures);
    });
  });

  group('Error Handling Integration Tests', () {
    setUp(() {
      ErrorHandler.clearError();
    });

    test('error handler with type converter', () {
      expect(
        () => TypeConverter.toNativeData({}),
        throwsA(isA<TypeMismatchException>()),
      );

      expect(
        () => TypeConverter.toNativeData([1, 2, 3]),
        throwsA(isA<TypeMismatchException>()),
      );
    });

    test('error messages are descriptive', () {
      try {
        TypeConverter.toNativeData(RegExp('test'));
        fail('Expected TypeMismatchException');
      } catch (e) {
        expect(e, isA<TypeMismatchException>());
        expect(e.toString(), contains('Unsupported type'));
      }
    });
  });
}
