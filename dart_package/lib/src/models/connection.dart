import 'dart:ffi';

import 'package:ffi/ffi.dart';

import '../ffi/bindings.dart';
import '../ffi/handles.dart';
import '../utils/error_handler.dart';

/// Represents a connection between two node ports in the flow graph.
///
/// Connections define the data flow pathways between nodes, linking an
/// output port on a source node to an input port on a target node.
/// Each connection has a unique identifier and maintains references to
/// the connected nodes and their specific ports.
class Connection {
  final ConnectionHandle _handle;

  Connection._(this._handle);

  /// Creates a Connection wrapper for an existing handle.
  ///
  /// This is used internally and should not be called directly.
  Connection.fromHandle(this._handle);

  /// Get the native handle for this connection.
  ///
  /// This is used internally for FFI calls.
  Pointer<FlowConnection> get handle => _handle.handle;

  /// Check if this connection handle is valid.
  bool get isValid => _handle.isValid;

  /// Get the reference count for this connection.
  int get refCount => _handle.refCount;

  /// Gets the unique identifier for this connection.
  ///
  /// Each connection has a UUID that uniquely identifies it within the graph.
  String get id {
    final idPtr = flowCore.native.flow_connection_get_id(handle);
    ErrorHandler.checkError();
    if (idPtr == nullptr) {
      throw const UnknownFlowException('Failed to get connection ID');
    }
    return idPtr.cast<Utf8>().toDartString();
  }

  /// Gets the UUID of the source node.
  ///
  /// This is the node that provides the output data for this connection.
  String get startNodeId {
    final nodeIdPtr = flowCore.native.flow_connection_get_start_node_id(handle);
    ErrorHandler.checkError();
    if (nodeIdPtr == nullptr) {
      throw const UnknownFlowException('Failed to get start node ID');
    }
    return nodeIdPtr.cast<Utf8>().toDartString();
  }

  /// Gets the key of the source port.
  ///
  /// This identifies which output port on the source node this connection originates from.
  String get startPortKey {
    final portPtr = flowCore.native.flow_connection_get_start_port(handle);
    ErrorHandler.checkError();
    if (portPtr == nullptr) {
      throw const UnknownFlowException('Failed to get start port key');
    }
    return portPtr.cast<Utf8>().toDartString();
  }

  /// Gets the UUID of the target node.
  ///
  /// This is the node that receives the input data from this connection.
  String get endNodeId {
    final nodeIdPtr = flowCore.native.flow_connection_get_end_node_id(handle);
    ErrorHandler.checkError();
    if (nodeIdPtr == nullptr) {
      throw const UnknownFlowException('Failed to get end node ID');
    }
    return nodeIdPtr.cast<Utf8>().toDartString();
  }

  /// Gets the key of the target port.
  ///
  /// This identifies which input port on the target node this connection connects to.
  String get endPortKey {
    final portPtr = flowCore.native.flow_connection_get_end_port(handle);
    ErrorHandler.checkError();
    if (portPtr == nullptr) {
      throw const UnknownFlowException('Failed to get end port key');
    }
    return portPtr.cast<Utf8>().toDartString();
  }

  /// Gets a human-readable description of this connection.
  ///
  /// Returns a string in the format "sourceNode:sourcePort -> targetNode:targetPort".
  String get description {
    try {
      return '${startNodeId.substring(0, 8)}:$startPortKey -> ${endNodeId.substring(0, 8)}:$endPortKey';
    } catch (e) {
      return 'Connection(id: ${id.substring(0, 8)})';
    }
  }

  /// Checks if this connection involves the specified node.
  ///
  /// Returns true if the given [nodeId] matches either the start or end node.
  bool involvesNode(String nodeId) {
    try {
      return startNodeId == nodeId || endNodeId == nodeId;
    } catch (e) {
      return false;
    }
  }

  /// Checks if this connection uses the specified port on a node.
  ///
  /// Returns true if the connection connects to the given [portKey] on the specified [nodeId].
  bool involvesPort(String nodeId, String portKey) {
    try {
      return (startNodeId == nodeId && startPortKey == portKey) ||
          (endNodeId == nodeId && endPortKey == portKey);
    } catch (e) {
      return false;
    }
  }

  /// Manually release the connection handle.
  ///
  /// This is typically not needed as the finalizer will handle cleanup
  /// automatically when the Connection is garbage collected.
  void dispose() {
    _handle.release();
  }

  @override
  String toString() =>
      'Connection(id: $id, $description, refCount: $refCount, isValid: $isValid)';

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is Connection && other.id == id;
  }

  @override
  int get hashCode => id.hashCode;
}
