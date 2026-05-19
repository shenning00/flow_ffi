import 'dart:async';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../ffi/bindings_generated.dart' as bindings;
import '../ffi/bindings.dart';
import '../ffi/handles.dart';
import '../models/node.dart';
import '../models/connection.dart';
import 'error_handler.dart';

/// Types of events that can be emitted
enum EventType {
  nodeAdded,
  nodeRemoved,
  nodesConnected,
  nodesDisconnected,
  graphError,
  nodeCompute,
  nodeError,
  nodeSetInput,
  nodeSetOutput,
}

/// Event data classes for type-safe event handling
abstract class EventData {}

class NodeEventData extends EventData {
  final Node node;
  NodeEventData(this.node);
}

class ConnectionEventData extends EventData {
  final Connection connection;
  ConnectionEventData(this.connection);
}

class ErrorEventData extends EventData {
  final String error;
  ErrorEventData(this.error);
}

class NodeDataEventData extends EventData {
  final Node node;
  final String portKey;
  final NodeDataHandle data;
  NodeDataEventData(this.node, this.portKey, this.data);
}

/// Event registration handle with automatic cleanup
class EventRegistration {
  final bindings.FlowEventRegistrationHandle _handle;
  final StreamController _controller;

  /// Callback id used to locate the retained [NativeCallable] in
  /// [EventManager]. Internal; defaults to -1 for backward compatibility
  /// with any external constructor callers (there are none in-tree).
  final int _callbackId;

  EventRegistration(this._handle, this._controller, [this._callbackId = -1]);

  /// Get the controller (for internal use)
  StreamController get controller => _controller;

  /// Unregister this event listener.
  ///
  /// Ordering matters: we unregister on the C side FIRST
  /// ([flowCore.flow_event_unregister]) so flow-core stops dispatching, and
  /// only THEN close the owning [NativeCallable]. Closing before the C-side
  /// unregister would silently drop a late in-flight native call (lost
  /// final event).
  void unregister() {
    if (_handle != nullptr) {
      flowCore.flow_event_unregister(_handle);
      EventManager.instance._closeCallback(_callbackId);
      _controller.close();
    }
  }

  /// Check if this registration is still valid
  bool get isValid {
    if (_handle == nullptr) return false;
    return flowCore.flow_event_is_valid(_handle);
  }
}

/// Manages native callback trampolines for event handling
class EventManager {
  static EventManager? _instance;

  static EventManager get instance {
    _instance ??= EventManager._();
    return _instance!;
  }

  EventManager._();

  // Map to keep callbacks alive.
  //
  // Stores NativeCallable.listener instances (not raw Pointers) so that the
  // callbacks are invokable from flow-core's BS::thread_pool worker threads.
  // The four event signatures differ, so the map is typed as the raw
  // `NativeCallable` and each entry is the appropriately-parameterised
  // `NativeCallable<...>` created at the registration site.
  final Map<int, NativeCallable> _callbacks = {};
  static int _callbackIdCounter = 0;

  /// Register a graph node added event listener
  EventRegistration registerGraphNodeAdded(
    GraphHandle graphHandle,
    StreamController<NodeEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    // Create native callback. NativeCallable.listener is invokable from any
    // thread (flow-core invokes from BS::thread_pool workers) and delivers
    // asynchronously to the owning isolate's event loop.
    final callback = NativeCallable<bindings.FlowNodeEventCallbackFunction>
        .listener(
      _onNodeEventCallback, // Static function
    );
    callback.keepIsolateAlive = false;

    // Store callback to prevent GC
    _callbacks[callbackId] = callback;

    // Register with native library
    final handle = flowCore.flow_graph_on_node_added(
      graphHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId), // Pass callback ID as user data
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node added event');
    }

    // Store the controller for this callback ID
    _nodeEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a graph node removed event listener
  EventRegistration registerGraphNodeRemoved(
    GraphHandle graphHandle,
    StreamController<NodeEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback = NativeCallable<bindings.FlowNodeEventCallbackFunction>
        .listener(
      _onNodeEventCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_node_removed(
      graphHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node removed event');
    }

    _nodeEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a graph nodes connected event listener
  EventRegistration registerGraphNodesConnected(
    GraphHandle graphHandle,
    StreamController<ConnectionEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        NativeCallable<bindings.FlowConnectionEventCallbackFunction>.listener(
      _onConnectionEventCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_nodes_connected(
      graphHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register nodes connected event');
    }

    _connectionEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a graph nodes disconnected event listener
  EventRegistration registerGraphNodesDisconnected(
    GraphHandle graphHandle,
    StreamController<ConnectionEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        NativeCallable<bindings.FlowConnectionEventCallbackFunction>.listener(
      _onConnectionEventCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_nodes_disconnected(
      graphHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register nodes disconnected event');
    }

    _connectionEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a graph error event listener
  EventRegistration registerGraphError(
    GraphHandle graphHandle,
    StreamController<ErrorEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        NativeCallable<bindings.FlowErrorEventCallbackFunction>.listener(
      _onErrorEventCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_error(
      graphHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register graph error event');
    }

    _errorEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a node compute event listener
  EventRegistration registerNodeCompute(
    NodeHandle nodeHandle,
    StreamController<NodeEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    // BORROWED-handle path: event_bridge.cpp:311 lambda captures the
    // registration-time `node` by value and re-delivers it on every event.
    // Must use the non-owning trampoline so the Dart finalizer does not
    // free a ref the C side never transferred. See
    // [_onNodeEventBorrowedCallback].
    final callback = NativeCallable<bindings.FlowNodeEventCallbackFunction>
        .listener(
      _onNodeEventBorrowedCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_compute(
      nodeHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node compute event');
    }

    _nodeEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a node error event listener
  EventRegistration registerNodeError(
    NodeHandle nodeHandle,
    StreamController<ErrorEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        NativeCallable<bindings.FlowErrorEventCallbackFunction>.listener(
      _onErrorEventCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_error(
      nodeHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node error event');
    }

    _errorEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a node set input event listener
  EventRegistration registerNodeSetInput(
    NodeHandle nodeHandle,
    StreamController<NodeDataEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        NativeCallable<bindings.FlowNodeDataEventCallbackFunction>.listener(
      _onNodeDataEventCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_set_input(
      nodeHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register node set input event');
    }

    _nodeDataEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  /// Register a node set output event listener
  EventRegistration registerNodeSetOutput(
    NodeHandle nodeHandle,
    StreamController<NodeDataEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        NativeCallable<bindings.FlowNodeDataEventCallbackFunction>.listener(
      _onNodeDataEventCallback,
    );
    callback.keepIsolateAlive = false;

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_set_output(
      nodeHandle.handle,
      callback.nativeFunction,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      callback.close();
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register node set output event');
    }

    _nodeDataEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller, callbackId);
  }

  // Controller storage for callbacks
  final Map<int, StreamController<NodeEventData>> _nodeEventControllers = {};
  final Map<int, StreamController<ConnectionEventData>>
      _connectionEventControllers = {};
  final Map<int, StreamController<ErrorEventData>> _errorEventControllers = {};
  final Map<int, StreamController<NodeDataEventData>>
      _nodeDataEventControllers = {};

  // Static callback functions (must be static for Pointer.fromFunction)

  static void _onNodeEventCallback(
      bindings.FlowNodeHandle node, Pointer<Void> userData) {
    try {
      final callbackId = userData.address;
      final controller =
          EventManager.instance._nodeEventControllers[callbackId];
      if (controller != null && !controller.isClosed) {
        final nodeHandle = NodeHandle(node.cast());
        controller.add(NodeEventData(Node.fromHandle(nodeHandle)));
      }
    } catch (e) {
      // Handle errors silently to prevent crashes in native callbacks
    }
  }

  /// Borrowed-handle variant of [_onNodeEventCallback].
  ///
  /// Used for per-node events whose C++ lambda re-delivers the
  /// registration-time node handle by value (event_bridge.cpp:311 — capture
  /// `[callback, user_data, node]`). That pointer's single ref is owned by
  /// the Dart `Node` created in Graph.addNode/getNode; the event only borrows
  /// it. Wrapping it OWNING would attach a GC finalizer that double-frees the
  /// underlying object (use-after-free / "node is not registered").
  ///
  /// The `if (!nodeHandle.isValid) return;` guard closes an async-race
  /// window: `NativeCallable.listener` delivers asynchronously on a later
  /// event-loop turn, so the underlying node may have been removed between
  /// the C-side event fire and Dart-side delivery. Forwarding an invalid
  /// borrowed handle to a consumer that calls e.g. `Node.id` would throw
  /// from native code.
  static void _onNodeEventBorrowedCallback(
      bindings.FlowNodeHandle node, Pointer<Void> userData) {
    try {
      final callbackId = userData.address;
      final controller =
          EventManager.instance._nodeEventControllers[callbackId];
      if (controller != null && !controller.isClosed) {
        final nodeHandle = NodeHandle.fromExisting(node.cast());
        if (!nodeHandle.isValid) return;
        controller.add(NodeEventData(Node.fromHandle(nodeHandle)));
      }
    } catch (e) {
      // Handle errors silently to prevent crashes in native callbacks
    }
  }

  static void _onConnectionEventCallback(
      bindings.FlowConnectionHandle conn, Pointer<Void> userData) {
    try {
      final callbackId = userData.address;
      final controller =
          EventManager.instance._connectionEventControllers[callbackId];
      if (controller != null && !controller.isClosed) {
        final connHandle = ConnectionHandle(conn.cast());
        controller.add(ConnectionEventData(Connection.fromHandle(connHandle)));
      }
    } catch (e) {
      // Handle errors silently to prevent crashes in native callbacks
    }
  }

  static void _onErrorEventCallback(
      Pointer<Char> error, Pointer<Void> userData) {
    try {
      // Per the shared ABI contract, `error` is now heap-allocated by the
      // C++ side (new char[]) and OWNED BY THE CALLEE. Convert to a Dart
      // string and free it FIRST, before any early return on a
      // null/closed controller, otherwise the heap buffer leaks.
      String? errorStr;
      if (error != nullptr) {
        errorStr = error.cast<Utf8>().toDartString();
        flowCore.flow_free_string(error);
      }

      final callbackId = userData.address;
      final controller =
          EventManager.instance._errorEventControllers[callbackId];
      if (errorStr != null && controller != null && !controller.isClosed) {
        controller.add(ErrorEventData(errorStr));
      }
    } catch (e) {
      // Handle errors silently to prevent crashes in native callbacks
    }
  }

  static void _onNodeDataEventCallback(
    bindings.FlowNodeHandle node,
    Pointer<Char> portKey,
    bindings.FlowNodeDataHandle data,
    Pointer<Void> userData,
  ) {
    try {
      // Per the shared ABI contract, only `portKey` is now heap-allocated
      // by the C++ side (new char[]) and OWNED BY THE CALLEE. Convert it to
      // a Dart string and free it FIRST, before any early return (closed
      // controller, invalid borrowed node), otherwise the heap buffer leaks.
      String? portKeyStr;
      if (portKey != nullptr) {
        portKeyStr = portKey.cast<Utf8>().toDartString();
        flowCore.flow_free_string(portKey);
      }

      final callbackId = userData.address;
      final controller =
          EventManager.instance._nodeDataEventControllers[callbackId];
      if (portKeyStr != null && controller != null && !controller.isClosed) {
        // `node` is BORROWED: event_bridge.cpp:375/412 capture the
        // registration-time node handle by value and re-deliver it. The
        // Dart Node from Graph.addNode/getNode owns its single ref; the
        // event must NOT attach a finalizer to it (use-after-free).
        final nodeHandle = NodeHandle.fromExisting(node.cast());
        // Async-race guard: `NativeCallable.listener` delivers on a later
        // event-loop turn, so the borrowed node may have been removed
        // between the C-side event fire and Dart-side delivery. portKey
        // has already been freed above, so an early return here does not
        // leak. The freshly-created `data` handle is dropped without a
        // wrapper so its ref-count is released by the disposal below.
        if (!nodeHandle.isValid) {
          // `data` is FRESH per event (event_bridge.cpp:377/414): C++
          // minted a new handle whose single ref was transferred to Dart.
          // If we ignore the event we MUST release that ref ourselves —
          // otherwise we leak one SharedNodeData per same-turn removal.
          if (data != nullptr) {
            NodeDataHandle(data.cast()).dispose();
          }
          return;
        }
        // `data` is FRESH: event_bridge.cpp:377/414
        // create_handle<SharedNodeData>(data) mints a new handle whose one
        // ref is transferred to Dart. It MUST stay OWNING so the GC
        // finalizer releases it; making it fromExisting would leak.
        final dataHandle = NodeDataHandle(data.cast());
        controller.add(
          NodeDataEventData(
            Node.fromHandle(nodeHandle),
            portKeyStr,
            dataHandle,
          ),
        );
      }
    } catch (e) {
      // Handle errors silently to prevent crashes in native callbacks
    }
  }

  /// Close and drop the retained [NativeCallable] for [callbackId].
  ///
  /// Must only be called AFTER the C-side `flow_event_unregister` so a late
  /// in-flight native call cannot target a closed callable.
  void _closeCallback(int callbackId) {
    final callback = _callbacks.remove(callbackId);
    callback?.close();
  }

  /// Cleanup all event registrations.
  ///
  /// Ordering: unregister every still-live registration on the C side FIRST
  /// (so flow-core stops dispatching from its worker threads), THEN close
  /// the owning [NativeCallable]s, THEN close controllers and clear maps.
  void cleanup() {
    // Close all retained native callables. By the time cleanup() runs the
    // C side has been told to stop dispatching (registrations unregistered
    // individually, or the native library is being torn down), so it is
    // safe to close every callable here.
    for (final callback in _callbacks.values) {
      callback.close();
    }
    _callbacks.clear();

    for (final controller in _nodeEventControllers.values) {
      if (!controller.isClosed) controller.close();
    }
    for (final controller in _connectionEventControllers.values) {
      if (!controller.isClosed) controller.close();
    }
    for (final controller in _errorEventControllers.values) {
      if (!controller.isClosed) controller.close();
    }
    for (final controller in _nodeDataEventControllers.values) {
      if (!controller.isClosed) controller.close();
    }

    _nodeEventControllers.clear();
    _connectionEventControllers.clear();
    _errorEventControllers.clear();
    _nodeDataEventControllers.clear();
  }
}
