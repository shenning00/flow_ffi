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

  EventRegistration(this._handle, this._controller);

  /// Get the controller (for internal use)
  StreamController get controller => _controller;

  /// Unregister this event listener
  void unregister() {
    if (_handle != nullptr) {
      flowCore.flow_event_unregister(_handle);
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

  // Map to keep callbacks alive
  final Map<int, Pointer<NativeFunction>> _callbacks = {};
  static int _callbackIdCounter = 0;

  /// Register a graph node added event listener
  EventRegistration registerGraphNodeAdded(
    GraphHandle graphHandle,
    StreamController<NodeEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    // Create native callback
    final callback =
        Pointer.fromFunction<bindings.FlowNodeEventCallbackFunction>(
      _onNodeEventCallback, // Static function
    );

    // Store callback to prevent GC
    _callbacks[callbackId] = callback;

    // Register with native library
    final handle = flowCore.flow_graph_on_node_added(
      graphHandle.handle,
      callback,
      Pointer.fromAddress(callbackId), // Pass callback ID as user data
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node added event');
    }

    // Store the controller for this callback ID
    _nodeEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a graph node removed event listener
  EventRegistration registerGraphNodeRemoved(
    GraphHandle graphHandle,
    StreamController<NodeEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowNodeEventCallbackFunction>(
      _onNodeEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_node_removed(
      graphHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node removed event');
    }

    _nodeEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a graph nodes connected event listener
  EventRegistration registerGraphNodesConnected(
    GraphHandle graphHandle,
    StreamController<ConnectionEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowConnectionEventCallbackFunction>(
      _onConnectionEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_nodes_connected(
      graphHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register nodes connected event');
    }

    _connectionEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a graph nodes disconnected event listener
  EventRegistration registerGraphNodesDisconnected(
    GraphHandle graphHandle,
    StreamController<ConnectionEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowConnectionEventCallbackFunction>(
      _onConnectionEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_nodes_disconnected(
      graphHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register nodes disconnected event');
    }

    _connectionEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a graph error event listener
  EventRegistration registerGraphError(
    GraphHandle graphHandle,
    StreamController<ErrorEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowErrorEventCallbackFunction>(
      _onErrorEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_graph_on_error(
      graphHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register graph error event');
    }

    _errorEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a node compute event listener
  EventRegistration registerNodeCompute(
    NodeHandle nodeHandle,
    StreamController<NodeEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowNodeEventCallbackFunction>(
      _onNodeEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_compute(
      nodeHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node compute event');
    }

    _nodeEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a node error event listener
  EventRegistration registerNodeError(
    NodeHandle nodeHandle,
    StreamController<ErrorEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowErrorEventCallbackFunction>(
      _onErrorEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_error(
      nodeHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException('Failed to register node error event');
    }

    _errorEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a node set input event listener
  EventRegistration registerNodeSetInput(
    NodeHandle nodeHandle,
    StreamController<NodeDataEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowNodeDataEventCallbackFunction>(
      _onNodeDataEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_set_input(
      nodeHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register node set input event');
    }

    _nodeDataEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
  }

  /// Register a node set output event listener
  EventRegistration registerNodeSetOutput(
    NodeHandle nodeHandle,
    StreamController<NodeDataEventData> controller,
  ) {
    final callbackId = _callbackIdCounter++;

    final callback =
        Pointer.fromFunction<bindings.FlowNodeDataEventCallbackFunction>(
      _onNodeDataEventCallback,
    );

    _callbacks[callbackId] = callback;

    final handle = flowCore.flow_node_on_set_output(
      nodeHandle.handle,
      callback,
      Pointer.fromAddress(callbackId),
    );

    ErrorHandler.checkError();
    if (handle == nullptr) {
      _callbacks.remove(callbackId);
      throw const UnknownFlowException(
          'Failed to register node set output event');
    }

    _nodeDataEventControllers[callbackId] = controller;

    return EventRegistration(handle, controller);
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
      final callbackId = userData.address;
      final controller =
          EventManager.instance._errorEventControllers[callbackId];
      if (controller != null && !controller.isClosed) {
        final errorStr = error.cast<Utf8>().toDartString();
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
      final callbackId = userData.address;
      final controller =
          EventManager.instance._nodeDataEventControllers[callbackId];
      if (controller != null && !controller.isClosed) {
        final nodeHandle = NodeHandle(node.cast());
        final portKeyStr = portKey.cast<Utf8>().toDartString();
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

  /// Cleanup all event registrations
  void cleanup() {
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
    _callbacks.clear();
  }
}
