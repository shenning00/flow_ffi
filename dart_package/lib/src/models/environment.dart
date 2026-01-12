import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../ffi/bindings.dart';
import '../ffi/handles.dart';
import '../utils/error_handler.dart';
import 'factory.dart';

/// Represents the execution environment for flow graphs.
///
/// The Environment manages the thread pool for concurrent task execution,
/// provides access to the NodeFactory for node creation, and offers
/// utilities for system environment variable access.
class Environment {
  final EnvHandle _handle;

  Environment._(this._handle);

  /// Creates a new environment with the specified maximum number of threads.
  ///
  /// [maxThreads] must be positive. Defaults to 4 if not specified.
  ///
  /// Throws [InvalidArgumentException] if maxThreads is not positive.
  /// Throws [OutOfMemoryException] if environment creation fails.
  factory Environment({int maxThreads = 4}) {
    if (maxThreads <= 0) {
      throw const InvalidArgumentException('maxThreads must be positive');
    }

    final handle = flowCore.native.flow_env_create(maxThreads);
    if (handle == nullptr) {
      ErrorHandler.checkError();
      throw const UnknownFlowException('Failed to create environment');
    }

    return Environment._(EnvHandle(handle));
  }

  /// Creates an Environment wrapper for an existing handle.
  ///
  /// This is used internally and should not be called directly.
  Environment.fromHandle(this._handle);

  /// Get the native handle for this environment.
  ///
  /// This is used internally for FFI calls.
  Pointer<FlowEnv> get handle => _handle.handle;

  /// Check if this environment handle is valid.
  bool get isValid => _handle.isValid;

  /// Get the reference count for this environment.
  int get refCount => _handle.refCount;

  /// Manually retain the handle (increase reference count).
  void retain() => _handle.retain();

  /// Manually release the handle (decrease reference count).
  void release() => _handle.release();

  /// Gets the NodeFactory associated with this environment.
  ///
  /// The NodeFactory can be used to create nodes, query available node types,
  /// and check type conversions.
  ///
  /// Returns a new [NodeFactory] instance each time it's called, but all
  /// instances reference the same underlying factory.
  ///
  /// Throws [InvalidHandleException] if the environment handle is invalid.
  NodeFactory get factory {
    final factoryHandle = flowCore.native.flow_env_get_factory(_handle.handle);
    if (factoryHandle == nullptr) {
      ErrorHandler.checkError();
      throw const InvalidHandleException(
          'Failed to get factory from environment');
    }

    return NodeFactory.fromHandle(FactoryHandle(factoryHandle));
  }

  /// Waits for all queued tasks in the thread pool to complete.
  ///
  /// This is a blocking operation that will wait until all tasks that have
  /// been submitted to the environment's thread pool are finished executing.
  ///
  /// Throws [InvalidHandleException] if the environment handle is invalid.
  void wait() {
    final result = flowCore.native.flow_env_wait(_handle.handle);
    ErrorHandler.checkErrorCode(result);
  }

  /// Gets the value of a system environment variable.
  ///
  /// [name] is the name of the environment variable to retrieve.
  ///
  /// Returns the value of the environment variable, or null if it doesn't exist.
  ///
  /// Throws [InvalidArgumentException] if name is null or empty.
  /// Throws [InvalidHandleException] if the environment handle is invalid.
  String? getEnvironmentVariable(String name) {
    if (name.isEmpty) {
      throw const InvalidArgumentException(
          'Environment variable name cannot be empty');
    }

    final cName = name.toNativeUtf8();
    try {
      final resultPtr =
          flowCore.native.flow_env_get_var(_handle.handle, cName.cast<Char>());
      if (resultPtr == nullptr) {
        // Check if there was an error or if the variable just doesn't exist
        final errorInfo = ErrorHandler.getLastError();
        if (errorInfo.hasError) {
          ErrorHandler
              .checkError(); // This will throw the appropriate exception
        }
        return null; // Variable doesn't exist
      }

      final result = resultPtr.cast<Utf8>().toDartString();
      flowCore.native.flow_free_string(resultPtr);

      // Return null for empty strings (non-existent variables)
      return result.isEmpty ? null : result;
    } finally {
      calloc.free(cName);
    }
  }

  /// Manually release the environment handle.
  ///
  /// This is typically not needed as the finalizer will handle cleanup
  /// automatically when the Environment is garbage collected.
  void dispose() {
    _handle.release();
  }

  @override
  String toString() => 'Environment(refCount: $refCount, isValid: $isValid)';
}
