import 'dart:ffi';
import 'dart:io';

import 'bindings_generated.dart' as bindings;

/// The dynamic library in which the symbols for [FlowCoreBindings] can be found.
final DynamicLibrary _dylib = () {
  if (Platform.isMacOS || Platform.isIOS) {
    try {
      return DynamicLibrary.open('./libflow_ffi.dylib');
    } catch (e) {
      return DynamicLibrary.open('libflow_ffi.dylib');
    }
  }
  if (Platform.isAndroid || Platform.isLinux) {
    try {
      return DynamicLibrary.open('./libflow_ffi.so');
    } catch (e) {
      try {
        return DynamicLibrary.open('../build/libflow_ffi.so');
      } catch (e) {
        return DynamicLibrary.open('libflow_ffi.so');
      }
    }
  }
  if (Platform.isWindows) {
    try {
      return DynamicLibrary.open('./flow_ffi.dll');
    } catch (e) {
      return DynamicLibrary.open('flow_ffi.dll');
    }
  }
  throw UnsupportedError('Unknown platform: ${Platform.operatingSystem}');
}();

/// The bindings to the native functions in [_dylib].
final bindings.FlowCoreBindings _flowCore = bindings.FlowCoreBindings(_dylib);

/// Singleton for easy access to FlowCore bindings
class FlowCoreBindings {
  static FlowCoreBindings? _instance;

  FlowCoreBindings._();

  static FlowCoreBindings get instance {
    _instance ??= FlowCoreBindings._();
    return _instance!;
  }

  // Direct access to all the generated bindings
  bindings.FlowCoreBindings get native => _flowCore;

  // Convenience methods for the most common operations
  Pointer<Char> flow_get_last_error() => _flowCore.flow_get_last_error();
  void flow_clear_error() => _flowCore.flow_clear_error();
  void flow_set_error(int code, Pointer<Char> message) =>
      _flowCore.flow_set_error(code, message);

  // Environment
  Pointer<bindings.FlowEnv> flow_env_create(int maxThreads) =>
      _flowCore.flow_env_create(maxThreads);
  void flow_env_destroy(Pointer<bindings.FlowEnv> env) =>
      _flowCore.flow_env_destroy(env);
  Pointer<bindings.FlowNodeFactory> flow_env_get_factory(
          Pointer<bindings.FlowEnv> env) =>
      _flowCore.flow_env_get_factory(env);
  int flow_env_wait(Pointer<bindings.FlowEnv> env) =>
      _flowCore.flow_env_wait(env);
  Pointer<Char> flow_env_get_var(
          Pointer<bindings.FlowEnv> env, Pointer<Char> name) =>
      _flowCore.flow_env_get_var(env, name);

  // Graph
  Pointer<bindings.FlowGraph> flow_graph_create(
          Pointer<bindings.FlowEnv> env) =>
      _flowCore.flow_graph_create(env);
  void flow_graph_destroy(Pointer<bindings.FlowGraph> graph) =>
      _flowCore.flow_graph_destroy(graph);
  int flow_graph_run(Pointer<bindings.FlowGraph> graph) =>
      _flowCore.flow_graph_run(graph);
  Pointer<Char> flow_graph_save_to_json(Pointer<bindings.FlowGraph> graph) =>
      _flowCore.flow_graph_save_to_json(graph);
  int flow_graph_load_from_json(
          Pointer<bindings.FlowGraph> graph, Pointer<Char> json) =>
      _flowCore.flow_graph_load_from_json(graph, json);
  int flow_graph_get_nodes(
          Pointer<bindings.FlowGraph> graph,
          Pointer<Pointer<Pointer<bindings.FlowNode>>> nodes,
          Pointer<Size> count) =>
      _flowCore.flow_graph_get_nodes(graph, nodes, count);
  Pointer<bindings.FlowNode> flow_graph_add_node(
    Pointer<bindings.FlowGraph> graph,
    Pointer<Char> classId,
    Pointer<Char> name,
  ) =>
      _flowCore.flow_graph_add_node(graph, classId, name);
  int flow_graph_remove_node(
          Pointer<bindings.FlowGraph> graph, Pointer<Char> nodeId) =>
      _flowCore.flow_graph_remove_node(graph, nodeId);
  Pointer<bindings.FlowNode> flow_graph_get_node(
    Pointer<bindings.FlowGraph> graph,
    Pointer<Char> nodeId,
  ) =>
      _flowCore.flow_graph_get_node(graph, nodeId);
  Pointer<bindings.FlowConnection> flow_graph_connect_nodes(
    Pointer<bindings.FlowGraph> graph,
    Pointer<Char> sourceId,
    Pointer<Char> sourcePort,
    Pointer<Char> targetId,
    Pointer<Char> targetPort,
  ) =>
      _flowCore.flow_graph_connect_nodes(
          graph, sourceId, sourcePort, targetId, targetPort);
  int flow_graph_disconnect_nodes(
    Pointer<bindings.FlowGraph> graph,
    Pointer<Char> connectionId,
  ) =>
      _flowCore.flow_graph_disconnect_nodes(graph, connectionId);
  int flow_graph_clear(Pointer<bindings.FlowGraph> graph) =>
      _flowCore.flow_graph_clear(graph);

  // Module management
  Pointer<bindings.FlowModule> flow_module_create(
          Pointer<bindings.FlowNodeFactory> factory) =>
      _flowCore.flow_module_create(factory);
  void flow_module_destroy(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_destroy(module);
  int flow_module_load(
          Pointer<bindings.FlowModule> module, Pointer<Char> path) =>
      _flowCore.flow_module_load(module, path);
  int flow_module_unload(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_unload(module);
  int flow_module_register_nodes(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_register_nodes(module);
  int flow_module_unregister_nodes(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_unregister_nodes(module);
  bool flow_module_is_loaded(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_is_loaded(module);
  Pointer<Char> flow_module_get_name(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_get_name(module);
  Pointer<Char> flow_module_get_version(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_get_version(module);
  Pointer<Char> flow_module_get_author(Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_get_author(module);
  Pointer<Char> flow_module_get_description(
          Pointer<bindings.FlowModule> module) =>
      _flowCore.flow_module_get_description(module);

  // Handle validation and management
  bool flow_is_valid_handle(Pointer<Void> handle) =>
      _flowCore.flow_is_valid_handle(handle);
  void flow_retain_handle(Pointer<Void> handle) =>
      _flowCore.flow_retain_handle(handle);
  void flow_release_handle(Pointer<Void> handle) =>
      _flowCore.flow_release_handle(handle);
  int flow_get_ref_count(Pointer<Void> handle) =>
      _flowCore.flow_get_ref_count(handle);

  // Memory management
  void flow_free_string(Pointer<Char> str) => _flowCore.flow_free_string(str);
  void flow_free_handle_array(Pointer<Pointer<Void>> array) =>
      _flowCore.flow_free_handle_array(array);
  void flow_free_string_array(Pointer<Pointer<Char>> array, int count) =>
      _flowCore.flow_free_string_array(array, count);

  // Factory methods
  int flow_factory_get_categories(
    Pointer<bindings.FlowNodeFactory> factory,
    Pointer<Pointer<Pointer<Char>>> categories,
    Pointer<Size> count,
  ) =>
      _flowCore.flow_factory_get_categories(factory, categories, count);

  int flow_factory_get_node_classes(
    Pointer<bindings.FlowNodeFactory> factory,
    Pointer<Char> category,
    Pointer<Pointer<Pointer<Char>>> classes,
    Pointer<Size> count,
  ) =>
      _flowCore.flow_factory_get_node_classes(
          factory, category, classes, count);

  Pointer<Char> flow_factory_get_friendly_name(
    Pointer<bindings.FlowNodeFactory> factory,
    Pointer<Char> className,
  ) =>
      _flowCore.flow_factory_get_friendly_name(factory, className);

  bool flow_factory_is_convertible(
    Pointer<bindings.FlowNodeFactory> factory,
    Pointer<Char> fromType,
    Pointer<Char> toType,
  ) =>
      _flowCore.flow_factory_is_convertible(factory, fromType, toType);

  Pointer<bindings.FlowNode> flow_factory_create_node(
    Pointer<bindings.FlowNodeFactory> factory,
    Pointer<Char> className,
    Pointer<Char> uuid,
    Pointer<Char> name,
    Pointer<bindings.FlowEnv> env,
  ) =>
      _flowCore.flow_factory_create_node(factory, className, uuid, name, env);

  // Node methods
  Pointer<Char> flow_node_get_id(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_get_id(node);

  Pointer<Char> flow_node_get_name(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_get_name(node);

  Pointer<Char> flow_node_get_class(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_get_class(node);

  int flow_node_set_name(Pointer<bindings.FlowNode> node, Pointer<Char> name) =>
      _flowCore.flow_node_set_name(node, name);

  int flow_node_invoke_compute(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_invoke_compute(node);

  int flow_node_set_input_data(
    Pointer<bindings.FlowNode> node,
    Pointer<Char> portKey,
    Pointer<bindings.FlowNodeData> data,
  ) =>
      _flowCore.flow_node_set_input_data(node, portKey, data);

  Pointer<bindings.FlowNodeData> flow_node_get_input_data(
    Pointer<bindings.FlowNode> node,
    Pointer<Char> portKey,
  ) =>
      _flowCore.flow_node_get_input_data(node, portKey);

  Pointer<bindings.FlowNodeData> flow_node_get_output_data(
    Pointer<bindings.FlowNode> node,
    Pointer<Char> portKey,
  ) =>
      _flowCore.flow_node_get_output_data(node, portKey);

  int flow_node_clear_input_data(
    Pointer<bindings.FlowNode> node,
    Pointer<Char> portKey,
  ) =>
      _flowCore.flow_node_clear_input_data(node, portKey);

  int flow_node_clear_output_data(
    Pointer<bindings.FlowNode> node,
    Pointer<Char> portKey,
  ) =>
      _flowCore.flow_node_clear_output_data(node, portKey);

  bool flow_node_validate_required_inputs(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_validate_required_inputs(node);

  bool flow_node_has_connected_inputs(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_has_connected_inputs(node);

  bool flow_node_has_connected_outputs(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_has_connected_outputs(node);

  Pointer<Char> flow_node_save_to_json(Pointer<bindings.FlowNode> node) =>
      _flowCore.flow_node_save_to_json(node);

  int flow_node_load_from_json(
          Pointer<bindings.FlowNode> node, Pointer<Char> jsonStr) =>
      _flowCore.flow_node_load_from_json(node, jsonStr);

  // Connection methods
  Pointer<Char> flow_connection_get_id(Pointer<bindings.FlowConnection> conn) =>
      _flowCore.flow_connection_get_id(conn);

  Pointer<Char> flow_connection_get_start_node_id(
          Pointer<bindings.FlowConnection> conn) =>
      _flowCore.flow_connection_get_start_node_id(conn);

  Pointer<Char> flow_connection_get_start_port(
          Pointer<bindings.FlowConnection> conn) =>
      _flowCore.flow_connection_get_start_port(conn);

  Pointer<Char> flow_connection_get_end_node_id(
          Pointer<bindings.FlowConnection> conn) =>
      _flowCore.flow_connection_get_end_node_id(conn);

  Pointer<Char> flow_connection_get_end_port(
          Pointer<bindings.FlowConnection> conn) =>
      _flowCore.flow_connection_get_end_port(conn);

  // Data creation and access methods
  Pointer<bindings.FlowNodeData> flow_data_create_int(int value) =>
      _flowCore.flow_data_create_int(value);

  Pointer<bindings.FlowNodeData> flow_data_create_double(double value) =>
      _flowCore.flow_data_create_double(value);

  Pointer<bindings.FlowNodeData> flow_data_create_bool(bool value) =>
      _flowCore.flow_data_create_bool(value);

  Pointer<bindings.FlowNodeData> flow_data_create_string(Pointer<Char> value) =>
      _flowCore.flow_data_create_string(value);

  int flow_data_get_int(
          Pointer<bindings.FlowNodeData> data, Pointer<Int32> value) =>
      _flowCore.flow_data_get_int(data, value);

  int flow_data_get_double(
          Pointer<bindings.FlowNodeData> data, Pointer<Double> value) =>
      _flowCore.flow_data_get_double(data, value);

  int flow_data_get_bool(
          Pointer<bindings.FlowNodeData> data, Pointer<Bool> value) =>
      _flowCore.flow_data_get_bool(data, value);

  int flow_data_get_string(
          Pointer<bindings.FlowNodeData> data, Pointer<Pointer<Char>> value) =>
      _flowCore.flow_data_get_string(data, value);

  Pointer<Char> flow_data_get_type(Pointer<bindings.FlowNodeData> data) =>
      _flowCore.flow_data_get_type(data);

  void flow_data_destroy(Pointer<bindings.FlowNodeData> data) =>
      _flowCore.flow_data_destroy(data);

  Pointer<Char> flow_data_to_string(Pointer<bindings.FlowNodeData> data) =>
      _flowCore.flow_data_to_string(data);

  // Event callback methods
  Pointer<bindings.FlowEventRegistration> flow_graph_on_node_added(
    Pointer<bindings.FlowGraph> graph,
    Pointer<NativeFunction<bindings.FlowNodeEventCallbackFunction>> callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_graph_on_node_added(graph, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_graph_on_node_removed(
    Pointer<bindings.FlowGraph> graph,
    Pointer<NativeFunction<bindings.FlowNodeEventCallbackFunction>> callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_graph_on_node_removed(graph, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_graph_on_nodes_connected(
    Pointer<bindings.FlowGraph> graph,
    Pointer<NativeFunction<bindings.FlowConnectionEventCallbackFunction>>
        callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_graph_on_nodes_connected(graph, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_graph_on_nodes_disconnected(
    Pointer<bindings.FlowGraph> graph,
    Pointer<NativeFunction<bindings.FlowConnectionEventCallbackFunction>>
        callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_graph_on_nodes_disconnected(graph, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_graph_on_error(
    Pointer<bindings.FlowGraph> graph,
    Pointer<NativeFunction<bindings.FlowErrorEventCallbackFunction>> callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_graph_on_error(graph, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_node_on_compute(
    Pointer<bindings.FlowNode> node,
    Pointer<NativeFunction<bindings.FlowNodeEventCallbackFunction>> callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_node_on_compute(node, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_node_on_error(
    Pointer<bindings.FlowNode> node,
    Pointer<NativeFunction<bindings.FlowErrorEventCallbackFunction>> callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_node_on_error(node, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_node_on_set_input(
    Pointer<bindings.FlowNode> node,
    Pointer<NativeFunction<bindings.FlowNodeDataEventCallbackFunction>>
        callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_node_on_set_input(node, callback, userData);

  Pointer<bindings.FlowEventRegistration> flow_node_on_set_output(
    Pointer<bindings.FlowNode> node,
    Pointer<NativeFunction<bindings.FlowNodeDataEventCallbackFunction>>
        callback,
    Pointer<Void> userData,
  ) =>
      _flowCore.flow_node_on_set_output(node, callback, userData);

  int flow_event_unregister(
          Pointer<bindings.FlowEventRegistration> registration) =>
      _flowCore.flow_event_unregister(registration);

  bool flow_event_is_valid(
          Pointer<bindings.FlowEventRegistration> registration) =>
      _flowCore.flow_event_is_valid(registration);
}

/// Global reference for easy access to FlowCore bindings
final flowCore = FlowCoreBindings.instance;
