import 'package:test/test.dart';
import 'package:flow_ffi/src/ffi/bindings.dart';

void main() {
  group('Library Loading Tests', () {
    test('FFI library can be loaded', () {
      // This test just verifies the library can be loaded
      expect(flowCore, isNotNull);
      print('✅ FFI library loaded successfully');
    });

    test('Basic FFI function is available', () {
      // Test that we can access at least one function
      expect(flowCore.flow_env_create, isNotNull);
      print('✅ FFI functions are accessible');
    });
  });
}
