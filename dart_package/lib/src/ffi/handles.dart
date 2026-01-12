import 'dart:ffi';

import 'bindings.dart';
import 'bindings_generated.dart' as generated;

/// Base class for all FFI handle wrappers.
///
/// Provides reference counting and automatic memory management
/// using Dart finalizers.
abstract class HandleWrapper<T extends Opaque> {
  /// The native handle pointer
  final Pointer<T> _handle;

  /// Whether this wrapper owns the handle and should release it
  bool _ownsHandle = true;

  HandleWrapper(this._handle) {
    if (_handle == nullptr) {
      throw ArgumentError('Handle cannot be null');
    }

    // Set up finalizer for automatic cleanup
    _finalizer.attach(this, _handle.cast<Void>(), detach: this);
  }

  /// Creates a wrapper for an existing handle without taking ownership
  HandleWrapper.fromExisting(this._handle) : _ownsHandle = false {
    if (_handle == nullptr) {
      throw ArgumentError('Handle cannot be null');
    }
  }

  /// Get the native handle pointer
  Pointer<T> get handle => _handle;

  /// Check if this handle is valid
  bool get isValid => flowCore.flow_is_valid_handle(_handle.cast<Void>());

  /// Get the reference count for this handle
  int get refCount => flowCore.flow_get_ref_count(_handle.cast<Void>());

  /// Manually retain the handle (increase reference count)
  void retain() {
    flowCore.flow_retain_handle(_handle.cast<Void>());
  }

  /// Manually release the handle (decrease reference count)
  void release() {
    if (_ownsHandle) {
      flowCore.flow_release_handle(_handle.cast<Void>());
      _ownsHandle = false;
    }
  }

  /// Detach from the finalizer (prevents automatic cleanup)
  void detach() {
    _finalizer.detach(this);
    _ownsHandle = false;
  }

  static final Finalizer<Pointer<Void>> _finalizer =
      Finalizer<Pointer<Void>>((handle) {
    flowCore.flow_release_handle(handle);
  });
}

/// Handle wrapper for FlowEnv
class EnvHandle extends HandleWrapper<FlowEnv> {
  EnvHandle(super.handle);
  EnvHandle.fromExisting(super.handle) : super.fromExisting();
}

/// Handle wrapper for FlowGraph
class GraphHandle extends HandleWrapper<FlowGraph> {
  GraphHandle(super.handle);
  GraphHandle.fromExisting(super.handle) : super.fromExisting();
}

/// Handle wrapper for FlowNode
class NodeHandle extends HandleWrapper<FlowNode> {
  NodeHandle(super.handle);
  NodeHandle.fromExisting(super.handle) : super.fromExisting();
}

/// Handle wrapper for FlowConnection
class ConnectionHandle extends HandleWrapper<FlowConnection> {
  ConnectionHandle(super.handle);
  ConnectionHandle.fromExisting(super.handle) : super.fromExisting();
}

/// Handle wrapper for FlowNodeFactory
class FactoryHandle extends HandleWrapper<FlowNodeFactory> {
  FactoryHandle(super.handle);
  FactoryHandle.fromExisting(super.handle) : super.fromExisting();
}

/// Handle wrapper for FlowModule
class ModuleHandle extends HandleWrapper<FlowModule> {
  ModuleHandle(super.handle);
  ModuleHandle.fromExisting(super.handle) : super.fromExisting();
}

/// Handle wrapper for FlowNodeData
class NodeDataHandle extends HandleWrapper<FlowNodeData> {
  NodeDataHandle(super.handle);
  NodeDataHandle.fromExisting(super.handle) : super.fromExisting();

  /// Dispose of this handle (for compatibility)
  void dispose() {
    release();
  }
}

// Type aliases to use the generated types
typedef FlowEnv = generated.FlowEnv;
typedef FlowGraph = generated.FlowGraph;
typedef FlowNode = generated.FlowNode;
typedef FlowConnection = generated.FlowConnection;
typedef FlowNodeFactory = generated.FlowNodeFactory;
typedef FlowModule = generated.FlowModule;
typedef FlowNodeData = generated.FlowNodeData;
