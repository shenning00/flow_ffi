import 'dart:ffi';
import 'package:ffi/ffi.dart';
import '../ffi/handles.dart';
import '../ffi/bindings.dart';
import '../utils/error_handler.dart';

/// Exception thrown when a feature is not yet implemented
class UnimplementedException implements Exception {
  final String message;
  UnimplementedException(this.message);

  @override
  String toString() => 'UnimplementedException: $message';
}

/// Simplified type converter for basic data types.
///
/// This provides a minimal implementation that supports basic Dart types
/// (int, double, bool, String) using the FFI bridge.
class TypeConverter {
  /// Creates native data from a Dart value.
  ///
  /// Supports: int, double, bool, String
  /// Throws [TypeMismatchException] for unsupported types.
  static NodeDataHandle toNativeData(dynamic value) {
    Pointer<FlowNodeData> dataPtr;

    if (value is int) {
      dataPtr = flowCore.flow_data_create_int(value);
    } else if (value is double) {
      dataPtr = flowCore.flow_data_create_double(value);
    } else if (value is bool) {
      dataPtr = flowCore.flow_data_create_bool(value);
    } else if (value is String) {
      final cString = value.toNativeUtf8();
      try {
        dataPtr = flowCore.flow_data_create_string(cString.cast<Char>());
      } finally {
        calloc.free(cString);
      }
    } else {
      throw TypeMismatchException(
          'Unsupported type for conversion: ${value.runtimeType}');
    }

    if (dataPtr == nullptr) {
      ErrorHandler.checkError();
      throw const UnknownFlowException('Failed to create native data');
    }

    return NodeDataHandle(dataPtr);
  }

  /// Attempts to convert native data to the requested type.
  ///
  /// Supports: int, double, bool, String
  /// Returns null if the conversion fails.
  static T? fromNativeData<T>(NodeDataHandle dataHandle) {
    if (!dataHandle.isValid) {
      return null;
    }

    if (T == int) {
      final valuePtr = calloc<Int32>();
      try {
        final result = flowCore.flow_data_get_int(dataHandle.handle, valuePtr);
        if (result == 0) {
          // FLOW_SUCCESS
          return valuePtr.value as T;
        }
      } finally {
        calloc.free(valuePtr);
      }
    } else if (T == double) {
      final valuePtr = calloc<Double>();
      try {
        final result =
            flowCore.flow_data_get_double(dataHandle.handle, valuePtr);
        if (result == 0) {
          // FLOW_SUCCESS
          return valuePtr.value as T;
        }
      } finally {
        calloc.free(valuePtr);
      }
    } else if (T == bool) {
      final valuePtr = calloc<Bool>();
      try {
        final result = flowCore.flow_data_get_bool(dataHandle.handle, valuePtr);
        if (result == 0) {
          // FLOW_SUCCESS
          return valuePtr.value as T;
        }
      } finally {
        calloc.free(valuePtr);
      }
    } else if (T == String) {
      final valuePtrPtr = calloc<Pointer<Char>>();
      try {
        final result =
            flowCore.flow_data_get_string(dataHandle.handle, valuePtrPtr);
        if (result == 0) {
          // FLOW_SUCCESS
          final valuePtr = valuePtrPtr.value;
          if (valuePtr != nullptr) {
            final str = valuePtr.cast<Utf8>().toDartString();
            flowCore.flow_free_string(valuePtr);
            return str as T;
          }
        }
      } finally {
        calloc.free(valuePtrPtr);
      }
    }

    return null;
  }

  /// Gets a string representation of any Dart value.
  static String valueToString(dynamic value) {
    if (value == null) return 'null';
    if (value is String) return value;
    if (value is num) return value.toString();
    if (value is bool) return value.toString();
    if (value is List) return value.map(valueToString).join(', ');
    if (value is Map) {
      final entries = value.entries
          .map((e) => '${valueToString(e.key)}: ${valueToString(e.value)}');
      return '{${entries.join(', ')}}';
    }
    return value.toString();
  }

  /// Attempts to parse a string value to the requested type.
  static T? parseValue<T>(String value) {
    try {
      if (T == String) return value as T;
      if (T == int) return int.tryParse(value) as T?;
      if (T == double) return double.tryParse(value) as T?;
      if (T == bool) {
        if (value.toLowerCase() == 'true') return true as T;
        if (value.toLowerCase() == 'false') return false as T;
        return null;
      }
      return null;
    } catch (e) {
      return null;
    }
  }

  /// Gets the Dart type name for a value.
  static String getTypeName(dynamic value) {
    if (value == null) return 'null';
    return value.runtimeType.toString();
  }

  /// Validates that a value can be converted to the target type.
  static bool canConvertTo<T>(dynamic value) {
    try {
      if (T == String) return true; // Everything can be converted to string
      if (T == int)
        return value is int || (value is String && int.tryParse(value) != null);
      if (T == double)
        return value is num ||
            (value is String && double.tryParse(value) != null);
      if (T == bool)
        return value is bool ||
            (value is String &&
                ['true', 'false'].contains(value.toLowerCase()));
      return false;
    } catch (e) {
      return false;
    }
  }
}
