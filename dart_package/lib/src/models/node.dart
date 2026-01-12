import 'dart:ffi';
import 'dart:convert';
import 'dart:async';

import 'package:ffi/ffi.dart';

import '../ffi/bindings.dart';
import '../ffi/bindings_generated.dart' show FlowPortMetadata;
import '../ffi/handles.dart';
import '../utils/error_handler.dart';
import '../utils/event_manager.dart';
import '../utils/type_converter_simple.dart';

/// Metadata for a node port (received from flow-core via FFI).
///
/// This class provides access to port metadata including the interworking type
/// and default value information. The metadata is encoded as JSON from the C++ layer.
class PortMetadata {
  final String key;
  final String? interworkingValueJson;
  final bool hasDefault;

  const PortMetadata({
    required this.key,
    this.interworkingValueJson,
    required this.hasDefault,
  });

  /// Parse the JSON to extract type and value.
  ///
  /// Returns null if the JSON is null, empty, or invalid.
  Map<String, dynamic>? get parsedValue {
    if (interworkingValueJson == null || interworkingValueJson!.isEmpty) {
      return null;
    }
    try {
      return jsonDecode(interworkingValueJson!) as Map<String, dynamic>;
    } catch (e) {
      print('[PortMetadata] Failed to parse interworking JSON: $e');
      return null;
    }
  }

  /// Get the interworking type string.
  ///
  /// Returns one of: "integer", "float", "string", "boolean", "none"
  /// Returns null if the JSON is invalid or type field is missing.
  String? get interworkingType => parsedValue?['type'] as String?;

  /// Get the default value as a string from JSON.
  ///
  /// Returns null if there is no default value or JSON is invalid.
  String? get defaultValueString {
    final parsed = parsedValue;
    if (parsed == null) return null;
    final value = parsed['value'];
    return value?.toString();
  }

  /// Check if this port supports manual editing.
  ///
  /// Returns true if the port has an editable interworking type (not "none").
  bool get isEditable {
    final type = interworkingType;
    return type != null && type != 'none';
  }

  @override
  String toString() =>
      'PortMetadata(key: $key, type: $interworkingType, hasDefault: $hasDefault, defaultValue: $defaultValueString)';

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is PortMetadata &&
        other.key == key &&
        other.interworkingValueJson == interworkingValueJson &&
        other.hasDefault == hasDefault;
  }

  @override
  int get hashCode => Object.hash(key, interworkingValueJson, hasDefault);
}

/// Represents a computational node in the flow graph.
///
/// Nodes are the fundamental processing units that transform data as it
/// flows through the graph. Each node has input and output ports for
/// data exchange and can perform computations when invoked.
class Node {
  final NodeHandle _handle;

  Node._(this._handle);

  /// Creates a Node wrapper for an existing handle.
  ///
  /// This is used internally and should not be called directly.
  Node.fromHandle(this._handle);

  /// Get the native handle for this node.
  ///
  /// This is used internally for FFI calls.
  Pointer<FlowNode> get handle => _handle.handle;

  /// Check if this node handle is valid.
  bool get isValid => _handle.isValid;

  /// Get the reference count for this node.
  int get refCount => _handle.refCount;

  /// Gets the unique identifier for this node.
  ///
  /// Returns the UUID string that uniquely identifies this node in the graph.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  String get id {
    final resultPtr = flowCore.flow_node_get_id(_handle.handle);
    if (resultPtr == nullptr) {
      ErrorHandler.checkError();
      throw const InvalidHandleException('Failed to get node ID');
    }

    final result = resultPtr.cast<Utf8>().toDartString();
    flowCore.flow_free_string(resultPtr);
    return result;
  }

  /// Gets the friendly name of this node.
  ///
  /// Returns the human-readable name that can be displayed in the UI.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  String get name {
    final resultPtr = flowCore.flow_node_get_name(_handle.handle);
    if (resultPtr == nullptr) {
      ErrorHandler.checkError();
      throw const InvalidHandleException('Failed to get node name');
    }

    final result = resultPtr.cast<Utf8>().toDartString();
    flowCore.flow_free_string(resultPtr);
    return result;
  }

  /// Sets the friendly name of this node.
  ///
  /// [value] is the new name for the node.
  ///
  /// Throws [InvalidArgumentException] if the name is empty.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  set name(String value) {
    if (value.isEmpty) {
      throw const InvalidArgumentException('Node name cannot be empty');
    }

    final cName = value.toNativeUtf8();
    try {
      final result =
          flowCore.flow_node_set_name(_handle.handle, cName.cast<Char>());
      ErrorHandler.checkErrorCode(result);
    } finally {
      calloc.free(cName);
    }
  }

  /// Gets the class name of this node.
  ///
  /// Returns the class name that was used to create this node.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  String get className {
    final resultPtr = flowCore.flow_node_get_class(_handle.handle);
    if (resultPtr == nullptr) {
      ErrorHandler.checkError();
      throw const InvalidHandleException('Failed to get node class');
    }

    final result = resultPtr.cast<Utf8>().toDartString();
    flowCore.flow_free_string(resultPtr);
    return result;
  }

  /// Triggers computation for this node.
  ///
  /// This invokes the node's computation logic, processing input data
  /// and producing output data. The computation is performed safely
  /// with automatic error handling.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  /// Throws [ComputationFailedException] if the computation fails.
  void compute() {
    final result = flowCore.flow_node_invoke_compute(_handle.handle);
    ErrorHandler.checkErrorCode(result);
  }

  /// Sets input data for a specific port.
  ///
  /// [portKey] is the identifier of the input port.
  /// [data] is the data to set. It will be converted to the appropriate type.
  ///
  /// Throws [InvalidArgumentException] if portKey is empty.
  /// Throws [PortNotFoundException] if the port doesn't exist.
  /// Throws [TypeMismatchException] if the data type doesn't match.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  void setInputData(String portKey, dynamic data) {
    if (portKey.isEmpty) {
      throw const InvalidArgumentException('Port key cannot be empty');
    }

    final dataHandle = TypeConverter.toNativeData(data);
    final cPortKey = portKey.toNativeUtf8();

    try {
      final result = flowCore.flow_node_set_input_data(
        _handle.handle,
        cPortKey.cast<Char>(),
        dataHandle.handle,
      );
      ErrorHandler.checkErrorCode(result);
    } finally {
      calloc.free(cPortKey);
      dataHandle.dispose();
    }
  }

  /// Gets input data from a specific port.
  ///
  /// [portKey] is the identifier of the input port.
  ///
  /// Returns the data cast to the requested type T, or null if no data
  /// is available or type conversion fails.
  ///
  /// Throws [InvalidArgumentException] if portKey is empty.
  /// Throws [PortNotFoundException] if the port doesn't exist.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  T? getInputData<T>(String portKey) {
    if (portKey.isEmpty) {
      throw const InvalidArgumentException('Port key cannot be empty');
    }

    final cPortKey = portKey.toNativeUtf8();
    try {
      final dataHandle = flowCore.flow_node_get_input_data(
        _handle.handle,
        cPortKey.cast<Char>(),
      );

      if (dataHandle == nullptr) {
        // Check if there was an error or if the port just has no data
        final errorMessage = ErrorHandler.getLastError();
        ErrorHandler.checkError(); // This will throw the appropriate exception
        return null; // No data in port
      }

      final dataWrapper = NodeDataHandle(dataHandle);
      try {
        return TypeConverter.fromNativeData<T>(dataWrapper);
      } finally {
        dataWrapper.dispose();
      }
    } finally {
      calloc.free(cPortKey);
    }
  }

  /// Gets output data from a specific port.
  ///
  /// [portKey] is the identifier of the output port.
  ///
  /// Returns the data cast to the requested type T, or null if no data
  /// is available or type conversion fails.
  ///
  /// Throws [InvalidArgumentException] if portKey is empty.
  /// Throws [PortNotFoundException] if the port doesn't exist.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  T? getOutputData<T>(String portKey) {
    if (portKey.isEmpty) {
      throw const InvalidArgumentException('Port key cannot be empty');
    }

    final cPortKey = portKey.toNativeUtf8();
    try {
      final dataHandle = flowCore.flow_node_get_output_data(
        _handle.handle,
        cPortKey.cast<Char>(),
      );

      if (dataHandle == nullptr) {
        // Check if there was an error or if the port just has no data
        final errorMessage = ErrorHandler.getLastError();
        ErrorHandler.checkError(); // This will throw the appropriate exception
        return null; // No data in port
      }

      final dataWrapper = NodeDataHandle(dataHandle);
      try {
        return TypeConverter.fromNativeData<T>(dataWrapper);
      } finally {
        dataWrapper.dispose();
      }
    } finally {
      calloc.free(cPortKey);
    }
  }

  /// Clears input data from a specific port.
  ///
  /// [portKey] is the identifier of the input port to clear.
  ///
  /// Throws [InvalidArgumentException] if portKey is empty.
  /// Throws [PortNotFoundException] if the port doesn't exist.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  void clearInputData(String portKey) {
    if (portKey.isEmpty) {
      throw const InvalidArgumentException('Port key cannot be empty');
    }

    final cPortKey = portKey.toNativeUtf8();
    try {
      final result = flowCore.flow_node_clear_input_data(
        _handle.handle,
        cPortKey.cast<Char>(),
      );
      ErrorHandler.checkErrorCode(result);
    } finally {
      calloc.free(cPortKey);
    }
  }

  /// Clears output data from a specific port.
  ///
  /// [portKey] is the identifier of the output port to clear.
  ///
  /// Throws [InvalidArgumentException] if portKey is empty.
  /// Throws [PortNotFoundException] if the port doesn't exist.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  void clearOutputData(String portKey) {
    if (portKey.isEmpty) {
      throw const InvalidArgumentException('Port key cannot be empty');
    }

    final cPortKey = portKey.toNativeUtf8();
    try {
      final result = flowCore.flow_node_clear_output_data(
        _handle.handle,
        cPortKey.cast<Char>(),
      );
      ErrorHandler.checkErrorCode(result);
    } finally {
      calloc.free(cPortKey);
    }
  }

  /// Validates that all required input ports have data.
  ///
  /// Returns true if all input ports have data, false otherwise.
  /// This can be used before calling [compute] to ensure the node
  /// is ready for computation.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  bool validateRequiredInputs() {
    final result = flowCore.flow_node_validate_required_inputs(_handle.handle);

    // Check for errors after the call
    final errorMessage = ErrorHandler.getLastError();
    ErrorHandler.checkError(); // This will throw the appropriate exception

    return result;
  }

  /// Checks if this node has any connected input ports.
  ///
  /// Returns true if at least one input port has data, indicating
  /// it's connected to another node's output.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  bool get hasConnectedInputs {
    final result = flowCore.flow_node_has_connected_inputs(_handle.handle);

    // Check for errors after the call
    final errorMessage = ErrorHandler.getLastError();
    ErrorHandler.checkError(); // This will throw the appropriate exception

    return result;
  }

  /// Checks if this node has any connected output ports.
  ///
  /// Returns true if at least one output port has data, indicating
  /// this node has produced output.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  bool get hasConnectedOutputs {
    final result = flowCore.flow_node_has_connected_outputs(_handle.handle);

    // Check for errors after the call
    final errorMessage = ErrorHandler.getLastError();
    ErrorHandler.checkError(); // This will throw the appropriate exception

    return result;
  }

  /// Gets a list of all input port keys for this node.
  ///
  /// Returns a list of port key strings that can be used with
  /// [getInputData] and [setInputData] methods.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  List<String> getInputPortKeys() {
    final keysPtr = calloc<Pointer<Pointer<Char>>>();
    final countPtr = calloc<Size>();

    try {
      final error = flowCore.native.flow_node_get_input_port_keys(
        _handle.handle,
        keysPtr,
        countPtr,
      );
      ErrorHandler.checkErrorCode(error);

      final count = countPtr.value;
      if (count == 0) {
        return [];
      }

      final keys = <String>[];
      final keysArray = keysPtr.value;

      for (int i = 0; i < count; i++) {
        final keyPtr = keysArray.elementAt(i).value;
        if (keyPtr != nullptr) {
          keys.add(keyPtr.cast<Utf8>().toDartString());
        }
      }

      return keys;
    } finally {
      // Clean up allocated memory
      final count = countPtr.value;
      if (count > 0 && keysPtr.value != nullptr) {
        final keysArray = keysPtr.value;
        for (int i = 0; i < count; i++) {
          final keyPtr = keysArray.elementAt(i).value;
          if (keyPtr != nullptr) {
            calloc.free(keyPtr);
          }
        }
        calloc.free(keysArray);
      }
      calloc.free(keysPtr);
      calloc.free(countPtr);
    }
  }

  /// Gets a list of all output port keys for this node.
  ///
  /// Returns a list of port key strings that can be used with
  /// [getOutputData] and [clearOutputData] methods.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  List<String> getOutputPortKeys() {
    final keysPtr = calloc<Pointer<Pointer<Char>>>();
    final countPtr = calloc<Size>();

    try {
      final error = flowCore.native.flow_node_get_output_port_keys(
        _handle.handle,
        keysPtr,
        countPtr,
      );
      ErrorHandler.checkErrorCode(error);

      final count = countPtr.value;
      if (count == 0) {
        return [];
      }

      final keys = <String>[];
      final keysArray = keysPtr.value;

      for (int i = 0; i < count; i++) {
        final keyPtr = keysArray.elementAt(i).value;
        if (keyPtr != nullptr) {
          keys.add(keyPtr.cast<Utf8>().toDartString());
        }
      }

      return keys;
    } finally {
      // Clean up allocated memory
      final count = countPtr.value;
      if (count > 0 && keysPtr.value != nullptr) {
        final keysArray = keysPtr.value;
        for (int i = 0; i < count; i++) {
          final keyPtr = keysArray.elementAt(i).value;
          if (keyPtr != nullptr) {
            calloc.free(keyPtr);
          }
        }
        calloc.free(keysArray);
      }
      calloc.free(keysPtr);
      calloc.free(countPtr);
    }
  }

  /// Serializes the node state to JSON.
  ///
  /// Returns a map containing the node's serialized state that can
  /// be converted to JSON for persistence or transmission.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  Map<String, dynamic> saveToJson() {
    final jsonPtr = flowCore.flow_node_save_to_json(_handle.handle);
    if (jsonPtr == nullptr) {
      ErrorHandler.checkError();
      throw const UnknownFlowException('Failed to serialize node');
    }

    final jsonString = jsonPtr.cast<Utf8>().toDartString();
    flowCore.flow_free_string(jsonPtr);

    return json.decode(jsonString) as Map<String, dynamic>;
  }

  /// Restores the node state from JSON.
  ///
  /// [jsonData] is a map containing the serialized node state.
  ///
  /// Throws [InvalidArgumentException] if jsonData is invalid.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  void loadFromJson(Map<String, dynamic> jsonData) {
    final jsonString = json.encode(jsonData);
    final cJsonString = jsonString.toNativeUtf8();

    try {
      final result = flowCore.flow_node_load_from_json(
        _handle.handle,
        cJsonString.cast<Char>(),
      );
      ErrorHandler.checkErrorCode(result);
    } finally {
      calloc.free(cJsonString);
    }
  }

  // Event streams for node events
  StreamController<NodeEventData>? _computeController;
  StreamController<ErrorEventData>? _errorController;
  StreamController<NodeDataEventData>? _setInputController;
  StreamController<NodeDataEventData>? _setOutputController;

  // Event registrations for cleanup
  final List<EventRegistration> _eventRegistrations = [];

  /// Stream of compute events.
  ///
  /// Emits a [NodeEventData] whenever this node's compute method is called.
  Stream<NodeEventData> get onCompute {
    _computeController ??= StreamController<NodeEventData>.broadcast();

    // Register event if not already done
    if (!_eventRegistrations
        .any((reg) => reg.controller == _computeController)) {
      final registration = EventManager.instance.registerNodeCompute(
        _handle,
        _computeController!,
      );
      _eventRegistrations.add(registration);
    }

    return _computeController!.stream;
  }

  /// Stream of error events.
  ///
  /// Emits an [ErrorEventData] whenever an error occurs in this node.
  Stream<ErrorEventData> get onError {
    _errorController ??= StreamController<ErrorEventData>.broadcast();

    if (!_eventRegistrations.any((reg) => reg.controller == _errorController)) {
      final registration = EventManager.instance.registerNodeError(
        _handle,
        _errorController!,
      );
      _eventRegistrations.add(registration);
    }

    return _errorController!.stream;
  }

  /// Stream of set input events.
  ///
  /// Emits a [NodeDataEventData] whenever input data is set on this node.
  Stream<NodeDataEventData> get onSetInput {
    _setInputController ??= StreamController<NodeDataEventData>.broadcast();

    if (!_eventRegistrations
        .any((reg) => reg.controller == _setInputController)) {
      final registration = EventManager.instance.registerNodeSetInput(
        _handle,
        _setInputController!,
      );
      _eventRegistrations.add(registration);
    }

    return _setInputController!.stream;
  }

  /// Stream of set output events.
  ///
  /// Emits a [NodeDataEventData] whenever output data is set on this node.
  Stream<NodeDataEventData> get onSetOutput {
    _setOutputController ??= StreamController<NodeDataEventData>.broadcast();

    if (!_eventRegistrations
        .any((reg) => reg.controller == _setOutputController)) {
      final registration = EventManager.instance.registerNodeSetOutput(
        _handle,
        _setOutputController!,
      );
      _eventRegistrations.add(registration);
    }

    return _setOutputController!.stream;
  }

  /// Get metadata for a specific input port.
  ///
  /// [portKey] is the identifier of the input port.
  ///
  /// Returns the port metadata containing type information and default value,
  /// or null if the port doesn't exist or metadata is not available.
  ///
  /// Throws [InvalidArgumentException] if portKey is empty.
  /// Throws [InvalidHandleException] if the node handle is invalid.
  PortMetadata? getInputPortMetadata(String portKey) {
    if (portKey.isEmpty) {
      throw const InvalidArgumentException('Port key cannot be empty');
    }

    final cPortKey = portKey.toNativeUtf8();
    final metadataPtr =
        calloc.allocate<FlowPortMetadata>(sizeOf<FlowPortMetadata>());

    try {
      final result = flowCore.native.flow_node_get_port_metadata(
        _handle.handle,
        cPortKey.cast<Char>(),
        metadataPtr.cast<FlowPortMetadata>(),
      );

      // If there was an error, check what it was
      if (result != 0) {
        ErrorHandler.checkErrorCode(result);
        return null; // Port not found or no metadata
      }

      // Extract data from C struct
      final key = metadataPtr.ref.key != nullptr
          ? metadataPtr.ref.key.cast<Utf8>().toDartString()
          : portKey;

      final interworkingValueJson = metadataPtr.ref.interworking_value_json !=
              nullptr
          ? metadataPtr.ref.interworking_value_json.cast<Utf8>().toDartString()
          : null;

      final hasDefault = metadataPtr.ref.has_default;

      // Create the Dart object before freeing the C memory
      final dartMetadata = PortMetadata(
        key: key,
        interworkingValueJson: interworkingValueJson,
        hasDefault: hasDefault,
      );

      // Free the C strings allocated by flow_node_get_port_metadata
      flowCore.native.flow_free_port_metadata(metadataPtr);

      return dartMetadata;
    } finally {
      calloc.free(cPortKey);
      calloc.free(metadataPtr);
    }
  }

  /// Get metadata for all input ports.
  ///
  /// Returns a list of PortMetadata objects for each input port on this node.
  /// The list will be empty if there are no input ports.
  ///
  /// Throws [InvalidHandleException] if the node handle is invalid.
  List<PortMetadata> getAllInputPortsMetadata() {
    final metadataArrayPtr = calloc.allocate<Pointer<FlowPortMetadata>>(
        sizeOf<Pointer<FlowPortMetadata>>());
    final countPtr = calloc.allocate<Size>(sizeOf<Size>());

    try {
      final error = flowCore.native.flow_node_get_input_ports_metadata(
        _handle.handle,
        metadataArrayPtr.cast<Pointer<FlowPortMetadata>>(),
        countPtr.cast<Size>(),
      );

      ErrorHandler.checkErrorCode(error);

      final count = countPtr.value;
      if (count == 0) {
        return [];
      }

      final metadataList = <PortMetadata>[];
      final metadataArray = metadataArrayPtr.value;

      for (int i = 0; i < count; i++) {
        final metadataItem = metadataArray + i;

        final key = metadataItem.ref.key != nullptr
            ? metadataItem.ref.key.cast<Utf8>().toDartString()
            : '';

        final interworkingValueJson =
            metadataItem.ref.interworking_value_json != nullptr
                ? metadataItem.ref.interworking_value_json
                    .cast<Utf8>()
                    .toDartString()
                : null;

        final hasDefault = metadataItem.ref.has_default;

        metadataList.add(PortMetadata(
          key: key,
          interworkingValueJson: interworkingValueJson,
          hasDefault: hasDefault,
        ));
      }

      // Free the array using the proper C function
      flowCore.native.flow_free_port_metadata_array(metadataArray, count);

      return metadataList;
    } finally {
      calloc.free(metadataArrayPtr);
      calloc.free(countPtr);
    }
  }

  /// Manually release the node handle and cleanup event registrations.
  ///
  /// This is typically not needed as the finalizer will handle cleanup
  /// automatically when the Node is garbage collected.
  void dispose() {
    // Unregister all event listeners
    for (final registration in _eventRegistrations) {
      registration.unregister();
    }
    _eventRegistrations.clear();

    // Close all stream controllers
    _computeController?.close();
    _errorController?.close();
    _setInputController?.close();
    _setOutputController?.close();

    _handle.release();
  }

  @override
  String toString() =>
      'Node(id: ${id.substring(0, 8)}..., name: $name, class: $className)';

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is Node && other.id == id;
  }

  @override
  int get hashCode => id.hashCode;
}
