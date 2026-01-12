import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'package:test/test.dart';
import 'package:flow_ffi/src/ffi/bindings.dart';
import 'package:flow_ffi/src/utils/error_handler.dart';

void main() {
  group('Memory Stress Tests', () {
    setUp(() {
      ErrorHandler.clearError();
    });

    test('repeated data type checking does not crash', () {
      // This test specifically targets the bug that was causing crashes
      // in the "data type checking" test
      for (int i = 0; i < 100; i++) {
        final intDataHandle = flowCore.flow_data_create_int(i);
        expect(intDataHandle.address, isNot(equals(0)));

        // This call was causing malloc(): unaligned tcache chunk detected
        final typePtr = flowCore.flow_data_get_type(intDataHandle);
        expect(typePtr.address, isNot(equals(0)));

        final typeName = typePtr.cast<Utf8>().toDartString();
        expect(typeName, contains('int'));

        flowCore.flow_free_string(typePtr);
        flowCore.flow_data_destroy(intDataHandle);
      }
    });

    test('repeated data to string conversion does not crash', () {
      // This test targets the ToString functionality
      for (int i = 0; i < 100; i++) {
        final intDataHandle = flowCore.flow_data_create_int(i);

        // This call could potentially crash if there are memory issues
        final stringPtr = flowCore.flow_data_to_string(intDataHandle);
        expect(stringPtr.address, isNot(equals(0)));

        final stringValue = stringPtr.cast<Utf8>().toDartString();
        expect(stringValue, equals(i.toString()));

        flowCore.flow_free_string(stringPtr);
        flowCore.flow_data_destroy(intDataHandle);
      }
    });

    test('mixed data operations do not cause memory corruption', () {
      // Create various types of data and perform operations
      for (int i = 0; i < 50; i++) {
        final intData = flowCore.flow_data_create_int(i);
        final doubleData = flowCore.flow_data_create_double(i * 1.5);
        final boolData = flowCore.flow_data_create_bool(i % 2 == 0);

        // Get types for all
        final intType = flowCore.flow_data_get_type(intData);
        final doubleType = flowCore.flow_data_get_type(doubleData);
        final boolType = flowCore.flow_data_get_type(boolData);

        // Convert to strings
        final intStr = flowCore.flow_data_to_string(intData);
        final doubleStr = flowCore.flow_data_to_string(doubleData);
        final boolStr = flowCore.flow_data_to_string(boolData);

        // Verify types are correct
        expect(intType.cast<Utf8>().toDartString(), contains('int'));
        expect(doubleType.cast<Utf8>().toDartString(), contains('double'));
        expect(boolType.cast<Utf8>().toDartString(), contains('bool'));

        // Clean up
        flowCore.flow_free_string(intType);
        flowCore.flow_free_string(doubleType);
        flowCore.flow_free_string(boolType);
        flowCore.flow_free_string(intStr);
        flowCore.flow_free_string(doubleStr);
        flowCore.flow_free_string(boolStr);
        flowCore.flow_data_destroy(intData);
        flowCore.flow_data_destroy(doubleData);
        flowCore.flow_data_destroy(boolData);
      }
    });

    test('string data with special characters does not crash', () {
      final testStrings = [
        'Hello, World!',
        'Unicode: \u00E9\u00E0\u00F1',
        'Special: !@#\$%^&*()',
        'Multi\nLine\nString',
        'Tab\tSeparated\tValues',
        '', // empty string
        'Very long string ' * 100, // long string
      ];

      for (final testStr in testStrings) {
        final cString = testStr.toNativeUtf8();

        try {
          final stringDataHandle =
              flowCore.flow_data_create_string(cString.cast<Char>());
          expect(stringDataHandle.address, isNot(equals(0)));

          // Get type - this was crashing before
          final typePtr = flowCore.flow_data_get_type(stringDataHandle);
          expect(typePtr.address, isNot(equals(0)));
          final typeName = typePtr.cast<Utf8>().toDartString();
          expect(typeName, contains('string'));
          flowCore.flow_free_string(typePtr);

          // Convert to string
          final valuePtrPtr = calloc<Pointer<Char>>();
          try {
            final result =
                flowCore.flow_data_get_string(stringDataHandle, valuePtrPtr);
            expect(result, equals(0)); // FLOW_SUCCESS

            final valuePtr = valuePtrPtr.value;
            expect(valuePtr.address, isNot(equals(0)));

            final resultString = valuePtr.cast<Utf8>().toDartString();
            expect(resultString, equals(testStr));

            flowCore.flow_free_string(valuePtr);
          } finally {
            calloc.free(valuePtrPtr);
            flowCore.flow_data_destroy(stringDataHandle);
          }
        } finally {
          calloc.free(cString);
        }
      }
    });
  });
}
