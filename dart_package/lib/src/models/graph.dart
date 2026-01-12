import 'dart:convert';
import 'dart:ffi';
import 'dart:async';

import 'package:ffi/ffi.dart';

import '../ffi/bindings.dart';
import '../ffi/handles.dart';
import '../utils/error_handler.dart';
import '../utils/event_manager.dart';
import 'connection.dart';
import 'environment.dart';
import 'node.dart';

/// Represents a computational graph containing nodes and their connections.
///
/// The Graph serves as the central orchestrator for the flow-based computation
/// system, managing a collection of nodes and their interconnections. It handles
/// execution coordination, data propagation, and lifecycle management.
class Graph {
  final GraphHandle _handle;

  Graph._(this._handle);

  /// Creates a new graph with the specified environment.
  ///
  /// The graph will use the provided environment for node creation and
  /// computation execution.
  factory Graph(Environment env) {
    final handle = flowCore.flow_graph_create(env.handle);
    ErrorHandler.checkError();
    if (handle == nullptr) {
      throw const UnknownFlowException('Failed to create graph');
    }
    return Graph._(GraphHandle(handle.cast()));
  }

  /// Creates a Graph wrapper for an existing handle.
  ///
  /// This is used internally and should not be called directly.
  Graph.fromHandle(this._handle);

  /// Get the native handle for this graph.
  ///
  /// This is used internally for FFI calls.
  Pointer<FlowGraph> get handle => _handle.handle;

  /// Check if this graph handle is valid.
  bool get isValid => _handle.isValid;

  /// Get the reference count for this graph.
  int get refCount => _handle.refCount;

  /// Adds a new node to the graph.
  ///
  /// Creates a node of the specified [classId] with the given [name] and
  /// adds it to the graph. Returns the created node instance.
  ///
  /// Throws [FlowException] if the node creation fails.
  Node addNode(String classId, [String? name]) {
    final nodeHandle = flowCore.native.flow_graph_add_node(
      handle,
      classId.toNativeUtf8().cast<Char>(),
      (name ?? 'Node').toNativeUtf8().cast<Char>(),
    );
    ErrorHandler.checkError();
    if (nodeHandle == nullptr) {
      throw UnknownFlowException('Failed to add node of class: $classId');
    }
    return Node.fromHandle(NodeHandle(nodeHandle.cast()));
  }

  /// Removes a node from the graph by its ID.
  ///
  /// Throws [FlowException] if the node removal fails.
  void removeNode(String nodeId) {
    final error = flowCore.native.flow_graph_remove_node(
      handle,
      nodeId.toNativeUtf8().cast<Char>(),
    );
    ErrorHandler.checkErrorCode(error);
  }

  /// Gets a node by its ID.
  ///
  /// Returns the node instance if found, throws [FlowException] otherwise.
  Node getNode(String nodeId) {
    final nodeHandle = flowCore.native.flow_graph_get_node(
      handle,
      nodeId.toNativeUtf8().cast<Char>(),
    );
    ErrorHandler.checkError();
    if (nodeHandle == nullptr) {
      throw NodeNotFoundException('Node not found with ID: $nodeId');
    }
    return Node.fromHandle(NodeHandle(nodeHandle.cast()));
  }

  /// Gets all nodes in the graph.
  ///
  /// Returns a list of all node instances in the graph.
  List<Node> getNodes() {
    final nodesPtr = calloc<Pointer<Pointer<FlowNode>>>();
    final countPtr = calloc<Size>();

    try {
      final error = flowCore.flow_graph_get_nodes(
        handle,
        nodesPtr,
        countPtr,
      );
      ErrorHandler.checkErrorCode(error);

      final count = countPtr.value;
      if (count == 0) {
        return [];
      }

      final nodes = <Node>[];
      final nodesArray = nodesPtr.value;

      for (int i = 0; i < count; i++) {
        final nodeHandle = nodesArray.elementAt(i).value;
        if (nodeHandle != nullptr) {
          nodes.add(Node.fromHandle(NodeHandle(nodeHandle.cast())));
        }
      }

      // Free the array (but not the individual handles - they're managed by Dart)
      flowCore.flow_free_handle_array(nodesArray.cast<Pointer<Void>>());

      return nodes;
    } finally {
      calloc.free(nodesPtr);
      calloc.free(countPtr);
    }
  }

  /// Checks if two nodes can be connected.
  ///
  /// Verifies whether the output port of the source node is compatible with
  /// the input port of the target node. This allows for pre-validation before
  /// attempting to create a connection.
  ///
  /// Returns `true` if the connection is possible, `false` otherwise.
  /// This method does not throw exceptions; it returns a boolean result.
  bool canConnect(
    String sourceId,
    String sourcePort,
    String targetId,
    String targetPort,
  ) {
    final result = flowCore.native.flow_graph_can_connect(
      handle,
      sourceId.toNativeUtf8().cast<Char>(),
      sourcePort.toNativeUtf8().cast<Char>(),
      targetId.toNativeUtf8().cast<Char>(),
      targetPort.toNativeUtf8().cast<Char>(),
    );
    return result;
  }

  /// Connects two nodes together.
  ///
  /// Creates a connection from the output port of the source node to the
  /// input port of the target node.
  ///
  /// Returns the created connection instance.
  /// Throws [FlowException] if the connection fails.
  Connection connectNodes(
    String sourceId,
    String sourcePort,
    String targetId,
    String targetPort,
  ) {
    final connHandle = flowCore.native.flow_graph_connect_nodes(
      handle,
      sourceId.toNativeUtf8().cast<Char>(),
      sourcePort.toNativeUtf8().cast<Char>(),
      targetId.toNativeUtf8().cast<Char>(),
      targetPort.toNativeUtf8().cast<Char>(),
    );
    ErrorHandler.checkError();
    if (connHandle == nullptr) {
      throw const ConnectionFailedException('Failed to connect nodes');
    }
    return Connection.fromHandle(ConnectionHandle(connHandle.cast()));
  }

  /// Disconnects nodes by removing the specified connection.
  ///
  /// Throws [FlowException] if the disconnection fails.
  void disconnectNodes(String connectionId) {
    final error = flowCore.native.flow_graph_disconnect_nodes(
      handle,
      connectionId.toNativeUtf8().cast<Char>(),
    );
    ErrorHandler.checkErrorCode(error);
  }

  /// Runs the computational graph.
  ///
  /// Executes all source nodes in the graph, causing the computation to
  /// propagate through the entire graph.
  ///
  /// Throws [FlowException] if the execution fails.
  void run() {
    final error = flowCore.flow_graph_run(handle);
    ErrorHandler.checkErrorCode(error);
  }

  /// Clears all nodes and connections from the graph.
  ///
  /// Throws [FlowException] if the clear operation fails.
  void clear() {
    final error = flowCore.native.flow_graph_clear(handle);
    ErrorHandler.checkErrorCode(error);
  }

  /// Saves the graph state to a JSON string.
  ///
  /// Returns a JSON representation of the graph that can be used to
  /// restore the graph state later.
  ///
  /// Throws [FlowException] if the serialization fails.
  String saveToJson() {
    final jsonPtr = flowCore.flow_graph_save_to_json(handle);
    ErrorHandler.checkError();
    if (jsonPtr == nullptr) {
      throw const UnknownFlowException('Failed to serialize graph to JSON');
    }

    try {
      final jsonString = jsonPtr.cast<Utf8>().toDartString();
      return jsonString;
    } finally {
      flowCore.flow_free_string(jsonPtr);
    }
  }

  /// Loads graph state from a JSON string.
  ///
  /// Restores the graph to the state represented by the provided JSON.
  /// This will clear the current graph contents.
  ///
  /// Throws [FlowException] if the deserialization fails.
  void loadFromJson(String jsonString) {
    final error = flowCore.flow_graph_load_from_json(
      handle,
      jsonString.toNativeUtf8().cast<Char>(),
    );
    ErrorHandler.checkErrorCode(error);
  }

  /// Saves the graph state to a JSON object.
  ///
  /// Returns a Map representation that can be easily serialized.
  Map<String, dynamic> saveToJsonObject() {
    final jsonString = saveToJson();
    return jsonDecode(jsonString) as Map<String, dynamic>;
  }

  /// Loads graph state from a JSON object.
  ///
  /// Restores the graph from a Map representation.
  void loadFromJsonObject(Map<String, dynamic> jsonObject) {
    final jsonString = jsonEncode(jsonObject);
    loadFromJson(jsonString);
  }

  // Event streams for graph events
  StreamController<NodeEventData>? _nodeAddedController;
  StreamController<NodeEventData>? _nodeRemovedController;
  StreamController<ConnectionEventData>? _nodesConnectedController;
  StreamController<ConnectionEventData>? _nodesDisconnectedController;
  StreamController<ErrorEventData>? _errorController;

  // Event registrations for cleanup
  final List<EventRegistration> _eventRegistrations = [];

  /// Stream of node added events.
  ///
  /// Emits a [NodeEventData] whenever a node is added to this graph.
  Stream<NodeEventData> get onNodeAdded {
    _nodeAddedController ??= StreamController<NodeEventData>.broadcast();

    // Register event if not already done
    if (!_eventRegistrations
        .any((reg) => reg.controller == _nodeAddedController)) {
      final registration = EventManager.instance.registerGraphNodeAdded(
        _handle,
        _nodeAddedController!,
      );
      _eventRegistrations.add(registration);
    }

    return _nodeAddedController!.stream;
  }

  /// Stream of node removed events.
  ///
  /// Emits a [NodeEventData] whenever a node is removed from this graph.
  Stream<NodeEventData> get onNodeRemoved {
    _nodeRemovedController ??= StreamController<NodeEventData>.broadcast();

    if (!_eventRegistrations
        .any((reg) => reg.controller == _nodeRemovedController)) {
      final registration = EventManager.instance.registerGraphNodeRemoved(
        _handle,
        _nodeRemovedController!,
      );
      _eventRegistrations.add(registration);
    }

    return _nodeRemovedController!.stream;
  }

  /// Stream of nodes connected events.
  ///
  /// Emits a [ConnectionEventData] whenever nodes are connected in this graph.
  Stream<ConnectionEventData> get onNodesConnected {
    _nodesConnectedController ??=
        StreamController<ConnectionEventData>.broadcast();

    if (!_eventRegistrations
        .any((reg) => reg.controller == _nodesConnectedController)) {
      final registration = EventManager.instance.registerGraphNodesConnected(
        _handle,
        _nodesConnectedController!,
      );
      _eventRegistrations.add(registration);
    }

    return _nodesConnectedController!.stream;
  }

  /// Stream of nodes disconnected events.
  ///
  /// Emits a [ConnectionEventData] whenever nodes are disconnected in this graph.
  Stream<ConnectionEventData> get onNodesDisconnected {
    _nodesDisconnectedController ??=
        StreamController<ConnectionEventData>.broadcast();

    if (!_eventRegistrations
        .any((reg) => reg.controller == _nodesDisconnectedController)) {
      final registration = EventManager.instance.registerGraphNodesDisconnected(
        _handle,
        _nodesDisconnectedController!,
      );
      _eventRegistrations.add(registration);
    }

    return _nodesDisconnectedController!.stream;
  }

  /// Stream of error events.
  ///
  /// Emits an [ErrorEventData] whenever an error occurs in this graph.
  Stream<ErrorEventData> get onError {
    _errorController ??= StreamController<ErrorEventData>.broadcast();

    if (!_eventRegistrations.any((reg) => reg.controller == _errorController)) {
      final registration = EventManager.instance.registerGraphError(
        _handle,
        _errorController!,
      );
      _eventRegistrations.add(registration);
    }

    return _errorController!.stream;
  }

  /// Manually release the graph handle and cleanup event registrations.
  ///
  /// This is typically not needed as the finalizer will handle cleanup
  /// automatically when the Graph is garbage collected.
  void dispose() {
    // Unregister all event listeners
    for (final registration in _eventRegistrations) {
      registration.unregister();
    }
    _eventRegistrations.clear();

    // Close all stream controllers
    _nodeAddedController?.close();
    _nodeRemovedController?.close();
    _nodesConnectedController?.close();
    _nodesDisconnectedController?.close();
    _errorController?.close();

    _handle.release();
  }

  @override
  String toString() => 'Graph(refCount: $refCount, isValid: $isValid)';
}
