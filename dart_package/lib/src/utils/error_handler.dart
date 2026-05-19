import 'dart:ffi';
import 'dart:convert';

import 'package:ffi/ffi.dart';

import '../ffi/bindings.dart';

/// Error codes from the flow_ffi library
enum FlowError {
  success(0),
  invalidHandle(-1),
  invalidArgument(-2),
  nodeNotFound(-3),
  portNotFound(-4),
  connectionFailed(-5),
  moduleLoadFailed(-6),
  computationFailed(-7),
  outOfMemory(-8),
  typeMismatch(-9),
  notImplemented(-10),
  unknown(-999);

  const FlowError(this.value);
  final int value;

  static FlowError fromValue(int value) {
    for (final error in FlowError.values) {
      if (error.value == value) {
        return error;
      }
    }
    return FlowError.unknown;
  }
}

/// Base exception class for flow_ffi errors
abstract class FlowException implements Exception {
  final FlowError errorCode;
  final String message;

  const FlowException(this.errorCode, this.message);

  @override
  String toString() => 'FlowException($errorCode): $message';
}

/// Exception for invalid handle operations
class InvalidHandleException extends FlowException {
  const InvalidHandleException(String message)
      : super(FlowError.invalidHandle, message);
}

/// Exception for invalid arguments
class InvalidArgumentException extends FlowException {
  const InvalidArgumentException(String message)
      : super(FlowError.invalidArgument, message);
}

/// Exception for node not found errors
class NodeNotFoundException extends FlowException {
  const NodeNotFoundException(String message)
      : super(FlowError.nodeNotFound, message);
}

/// Exception for port not found errors
class PortNotFoundException extends FlowException {
  const PortNotFoundException(String message)
      : super(FlowError.portNotFound, message);
}

/// Exception for connection failures
class ConnectionFailedException extends FlowException {
  const ConnectionFailedException(String message)
      : super(FlowError.connectionFailed, message);
}

/// Exception for module loading failures
class ModuleLoadFailedException extends FlowException {
  const ModuleLoadFailedException(String message)
      : super(FlowError.moduleLoadFailed, message);
}

/// Exception for computation failures
class ComputationFailedException extends FlowException {
  const ComputationFailedException(String message)
      : super(FlowError.computationFailed, message);
}

/// Exception for out of memory errors
class OutOfMemoryException extends FlowException {
  const OutOfMemoryException(String message)
      : super(FlowError.outOfMemory, message);
}

/// Exception for type mismatch errors
class TypeMismatchException extends FlowException {
  const TypeMismatchException(String message)
      : super(FlowError.typeMismatch, message);
}

/// Exception for not implemented features
class NotImplementedException extends FlowException {
  const NotImplementedException(String message)
      : super(FlowError.notImplemented, message);
}

/// Exception for unknown errors
class UnknownFlowException extends FlowException {
  const UnknownFlowException(String message)
      : super(FlowError.unknown, message);
}

/// Utility class for handling errors from the native library
class ErrorHandler {
  /// Check for errors after a native function call and throw appropriate exceptions
  static void checkError() {
    final errorMessage = flowCore.flow_get_last_error();
    if (errorMessage == nullptr) {
      // No pending error: nothing to do.
      return;
    }

    final message = errorMessage.cast<Utf8>().toDartString();
    // Read the numeric error code BEFORE clearing the error.
    final code = flowCore.flow_get_last_error_code();
    flowCore.flow_clear_error();

    // Classify by the numeric error CODE rather than substring-matching the
    // human-readable message (which is fragile and misclassifies errors).
    switch (FlowError.fromValue(code)) {
      case FlowError.invalidHandle:
        throw InvalidHandleException(message);
      case FlowError.invalidArgument:
        throw InvalidArgumentException(message);
      case FlowError.nodeNotFound:
        throw NodeNotFoundException(message);
      case FlowError.portNotFound:
        throw PortNotFoundException(message);
      case FlowError.connectionFailed:
        throw ConnectionFailedException(message);
      case FlowError.moduleLoadFailed:
        throw ModuleLoadFailedException(message);
      case FlowError.computationFailed:
        throw ComputationFailedException(message);
      case FlowError.outOfMemory:
        throw OutOfMemoryException(message);
      case FlowError.typeMismatch:
        throw TypeMismatchException(message);
      case FlowError.notImplemented:
        throw NotImplementedException(message);
      case FlowError.success:
      case FlowError.unknown:
        throw UnknownFlowException(message);
    }
  }

  /// Check if an error code indicates success
  static bool isSuccess(int errorCode) {
    return errorCode == FlowError.success.value;
  }

  /// Check error code and throw exception if not successful
  static void checkErrorCode(int errorCode) {
    if (!isSuccess(errorCode)) {
      checkError(); // Will throw appropriate exception
    }
  }

  /// Clear any pending error
  static void clearError() {
    flowCore.flow_clear_error();
  }

  /// Get the last error message without clearing it
  static ErrorInfo getLastError() {
    final errorMessage = flowCore.flow_get_last_error();
    if (errorMessage != nullptr) {
      final message = errorMessage.cast<Utf8>().toDartString();
      return ErrorInfo(
          message: message,
          code: FlowError.unknown); // We'd need error code from C++
    }
    return const ErrorInfo(message: '', code: FlowError.success);
  }
}

/// Contains error information from the native library
class ErrorInfo {
  final String message;
  final FlowError code;

  const ErrorInfo({required this.message, required this.code});

  bool get hasError => code != FlowError.success;
}
