import 'package:test/test.dart';
import 'package:flow_ffi/src/ffi/bindings.dart';
import 'package:ffi/ffi.dart';
import 'dart:ffi';

void main() {
  group('Port API Tests', () {
    test('New port API functions are accessible via native bindings', () {
      // Test that the new functions exist in the native FFI bindings
      expect(flowCore.native.flow_node_get_input_port_type, isNotNull);
      expect(flowCore.native.flow_node_get_output_port_type, isNotNull);
      expect(flowCore.native.flow_node_get_port_description, isNotNull);

      print('✅ All new port API functions are available in FFI bindings');
    });

    test('Port API functions have correct signatures', () {
      // Test that functions can be called (even if they fail due to null handles)
      try {
        // Call with null handles - should not crash, just return null
        final inputType = flowCore.native.flow_node_get_input_port_type(
          Pointer.fromAddress(0), // nullptr equivalent
          'test'.toNativeUtf8().cast(),
        );
        final outputType = flowCore.native.flow_node_get_output_port_type(
          Pointer.fromAddress(0),
          'test'.toNativeUtf8().cast(),
        );
        final description = flowCore.native.flow_node_get_port_description(
          Pointer.fromAddress(0),
          'test'.toNativeUtf8().cast(),
          true,
        );

        // These should all return null/zero address for invalid handles
        expect(inputType.address, equals(0));
        expect(outputType.address, equals(0));
        expect(description.address, equals(0));

        print(
            '✅ Port API functions have correct signatures and handle null inputs safely');
      } catch (e) {
        fail('Port API functions should handle null inputs gracefully: $e');
      }
    });
  });
}
