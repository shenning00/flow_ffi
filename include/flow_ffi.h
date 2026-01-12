#ifndef FLOW_FFI_H
#define FLOW_FFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Export macro for shared library
#ifndef FLOW_FFI_EXPORT
#    ifdef _WIN32
#        define FLOW_FFI_EXPORT __declspec(dllexport)
#    else
#        define FLOW_FFI_EXPORT __attribute__((visibility("default")))
#    endif
#endif

// Error codes
typedef enum FlowError {
    FLOW_SUCCESS = 0,
    FLOW_ERROR_INVALID_HANDLE = -1,
    FLOW_ERROR_INVALID_ARGUMENT = -2,
    FLOW_ERROR_NODE_NOT_FOUND = -3,
    FLOW_ERROR_PORT_NOT_FOUND = -4,
    FLOW_ERROR_CONNECTION_FAILED = -5,
    FLOW_ERROR_MODULE_LOAD_FAILED = -6,
    FLOW_ERROR_COMPUTATION_FAILED = -7,
    FLOW_ERROR_OUT_OF_MEMORY = -8,
    FLOW_ERROR_TYPE_MISMATCH = -9,
    FLOW_ERROR_NOT_IMPLEMENTED = -10,
    FLOW_ERROR_UNKNOWN = -999
} FlowError;

// Opaque handle types
typedef struct FlowGraph* FlowGraphHandle;
typedef struct FlowNode* FlowNodeHandle;
typedef struct FlowConnection* FlowConnectionHandle;
typedef struct FlowEnv* FlowEnvHandle;
typedef struct FlowNodeFactory* FlowNodeFactoryHandle;
typedef struct FlowModule* FlowModuleHandle;
typedef struct FlowNodeData* FlowNodeDataHandle;

// Result structure for operations that may fail
typedef struct FlowResult {
    FlowError error;
    void* data;
} FlowResult;

// String result for functions returning strings
typedef struct FlowStringResult {
    FlowError error;
    const char* data;
} FlowStringResult;

// Connection information for graph connection queries
typedef struct FlowConnectionInfo {
    const char* id;
    const char* source_node_id;
    const char* source_port_key;
    const char* target_node_id;
    const char* target_port_key;
} FlowConnectionInfo;

// ============================================================================
// Error Handling
// ============================================================================

// Get the last error message
FLOW_FFI_EXPORT const char* flow_get_last_error(void);

// Clear the last error
FLOW_FFI_EXPORT void flow_clear_error(void);

// Set custom error message
FLOW_FFI_EXPORT void flow_set_error(FlowError code, const char* message);

// ============================================================================
// Handle Management
// ============================================================================

// Validate if a handle is valid
FLOW_FFI_EXPORT bool flow_is_valid_handle(void* handle);

// Reference counting for handles
FLOW_FFI_EXPORT void flow_retain_handle(void* handle);
FLOW_FFI_EXPORT void flow_release_handle(void* handle);
FLOW_FFI_EXPORT int32_t flow_get_ref_count(void* handle);

// ============================================================================
// Environment Management
// ============================================================================

// Create a new environment with specified thread count
FLOW_FFI_EXPORT FlowEnvHandle flow_env_create(int32_t max_threads);

// Destroy an environment
FLOW_FFI_EXPORT void flow_env_destroy(FlowEnvHandle env);

// Get the node factory from environment
FLOW_FFI_EXPORT FlowNodeFactoryHandle flow_env_get_factory(FlowEnvHandle env);

// Wait for all tasks to complete
FLOW_FFI_EXPORT FlowError flow_env_wait(FlowEnvHandle env);

// Get system environment variable
FLOW_FFI_EXPORT const char* flow_env_get_var(FlowEnvHandle env, const char* name);

// ============================================================================
// Graph Management
// ============================================================================

// Create a new graph
FLOW_FFI_EXPORT FlowGraphHandle flow_graph_create(FlowEnvHandle env);

// Destroy a graph
FLOW_FFI_EXPORT void flow_graph_destroy(FlowGraphHandle graph);

// Add a node to the graph
FLOW_FFI_EXPORT FlowNodeHandle flow_graph_add_node(FlowGraphHandle graph, const char* class_id,
                                                   const char* name);

// Remove a node from the graph
FLOW_FFI_EXPORT FlowError flow_graph_remove_node(FlowGraphHandle graph, const char* node_id);

// Get a node by ID
FLOW_FFI_EXPORT FlowNodeHandle flow_graph_get_node(FlowGraphHandle graph, const char* node_id);

// Get all nodes
FLOW_FFI_EXPORT FlowError flow_graph_get_nodes(FlowGraphHandle graph, FlowNodeHandle** nodes,
                                               size_t* count);

// Connect two nodes
FLOW_FFI_EXPORT FlowConnectionHandle flow_graph_connect_nodes(FlowGraphHandle graph,
                                                              const char* source_id,
                                                              const char* source_port,
                                                              const char* target_id,
                                                              const char* target_port);

// Disconnect nodes
FLOW_FFI_EXPORT FlowError flow_graph_disconnect_nodes(FlowGraphHandle graph,
                                                      const char* connection_id);

// Get all connections in the graph
FLOW_FFI_EXPORT FlowError flow_graph_get_connections(FlowGraphHandle graph,
                                                     FlowConnectionInfo** connections,
                                                     size_t* count);

// Check if two nodes can be connected
FLOW_FFI_EXPORT bool flow_graph_can_connect(FlowGraphHandle graph, const char* source_id,
                                            const char* source_port, const char* target_id,
                                            const char* target_port);

// Run the graph
FLOW_FFI_EXPORT FlowError flow_graph_run(FlowGraphHandle graph);

// Clear all nodes and connections
FLOW_FFI_EXPORT FlowError flow_graph_clear(FlowGraphHandle graph);

// Serialization
FLOW_FFI_EXPORT char* flow_graph_save_to_json(FlowGraphHandle graph);
FLOW_FFI_EXPORT FlowError flow_graph_load_from_json(FlowGraphHandle graph, const char* json);

// Node serialization
FLOW_FFI_EXPORT char* flow_node_save_to_json(FlowNodeHandle node);
FLOW_FFI_EXPORT FlowError flow_node_load_from_json(FlowNodeHandle node, const char* json_str);

// ============================================================================
// Node Management
// ============================================================================

// Get node properties
FLOW_FFI_EXPORT const char* flow_node_get_id(FlowNodeHandle node);
FLOW_FFI_EXPORT const char* flow_node_get_name(FlowNodeHandle node);
FLOW_FFI_EXPORT const char* flow_node_get_class(FlowNodeHandle node);

// Set node name
FLOW_FFI_EXPORT FlowError flow_node_set_name(FlowNodeHandle node, const char* name);

// Port data management
FLOW_FFI_EXPORT FlowError flow_node_set_input_data(FlowNodeHandle node, const char* port_key,
                                                   FlowNodeDataHandle data);

FLOW_FFI_EXPORT FlowNodeDataHandle flow_node_get_input_data(FlowNodeHandle node,
                                                            const char* port_key);

FLOW_FFI_EXPORT FlowNodeDataHandle flow_node_get_output_data(FlowNodeHandle node,
                                                             const char* port_key);

FLOW_FFI_EXPORT FlowError flow_node_clear_input_data(FlowNodeHandle node, const char* port_key);

FLOW_FFI_EXPORT FlowError flow_node_clear_output_data(FlowNodeHandle node, const char* port_key);

// Computation
FLOW_FFI_EXPORT FlowError flow_node_invoke_compute(FlowNodeHandle node);
FLOW_FFI_EXPORT bool flow_node_validate_required_inputs(FlowNodeHandle node);

// Connection status
FLOW_FFI_EXPORT bool flow_node_has_connected_inputs(FlowNodeHandle node);
FLOW_FFI_EXPORT bool flow_node_has_connected_outputs(FlowNodeHandle node);

// Port introspection
FLOW_FFI_EXPORT FlowError flow_node_get_input_port_keys(FlowNodeHandle node, char*** port_keys,
                                                        size_t* count);

FLOW_FFI_EXPORT FlowError flow_node_get_output_port_keys(FlowNodeHandle node, char*** port_keys,
                                                         size_t* count);

// Get port type information
FLOW_FFI_EXPORT const char* flow_node_get_input_port_type(FlowNodeHandle node,
                                                          const char* port_key);

FLOW_FFI_EXPORT const char* flow_node_get_output_port_type(FlowNodeHandle node,
                                                           const char* port_key);

// Get port description/caption
FLOW_FFI_EXPORT const char*
flow_node_get_port_description(FlowNodeHandle node, const char* port_key, bool is_input_port);

// ============================================================================
// Port Metadata (for Optional Values)
// ============================================================================

// Port metadata structure for UI integration
// The interworking_value_json field contains JSON with type and default value
// Format: {"type":"string|integer|float|boolean|none","value":"<default_value>"}
// Examples:
//   {"type":"string","value":"/home/user/file.png"}
//   {"type":"integer","value":"640"}
//   {"type":"float","value":"2.5"}
//   {"type":"boolean","value":"true"}
//   {"type":"none"}  // Complex types, not editable
typedef struct FlowPortMetadata {
    const char* key;                     // Port identifier
    const char* interworking_value_json; // JSON string with type and value
    bool has_default;                    // Whether default value exists
} FlowPortMetadata;

// Get port metadata for a specific port
FLOW_FFI_EXPORT FlowError flow_node_get_port_metadata(FlowNodeHandle node, const char* port_key,
                                                      FlowPortMetadata* metadata);

// Get metadata for all input ports
FLOW_FFI_EXPORT FlowError flow_node_get_input_ports_metadata(FlowNodeHandle node,
                                                             FlowPortMetadata** metadata_array,
                                                             size_t* count);

// Free port metadata array
FLOW_FFI_EXPORT void flow_free_port_metadata_array(FlowPortMetadata* metadata_array, size_t count);

// Free single port metadata
FLOW_FFI_EXPORT void flow_free_port_metadata(FlowPortMetadata* metadata);

// ============================================================================
// Connection Management
// ============================================================================

// Get connection properties
FLOW_FFI_EXPORT const char* flow_connection_get_id(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_start_node_id(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_start_port(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_end_node_id(FlowConnectionHandle conn);
FLOW_FFI_EXPORT const char* flow_connection_get_end_port(FlowConnectionHandle conn);

// ============================================================================
// Node Factory Management
// ============================================================================

// Create node from factory
FLOW_FFI_EXPORT FlowNodeHandle flow_factory_create_node(FlowNodeFactoryHandle factory,
                                                        const char* class_name, const char* uuid,
                                                        const char* name, FlowEnvHandle env);

// Get available node categories
FLOW_FFI_EXPORT FlowError flow_factory_get_categories(FlowNodeFactoryHandle factory,
                                                      char*** categories, size_t* count);

// Get node classes in a category
FLOW_FFI_EXPORT FlowError flow_factory_get_node_classes(FlowNodeFactoryHandle factory,
                                                        const char* category, char*** classes,
                                                        size_t* count);

// Get friendly name for a node class
FLOW_FFI_EXPORT const char* flow_factory_get_friendly_name(FlowNodeFactoryHandle factory,
                                                           const char* class_name);

// Type conversion checking
FLOW_FFI_EXPORT bool flow_factory_is_convertible(FlowNodeFactoryHandle factory,
                                                 const char* from_type, const char* to_type);

// ============================================================================
// Module Management
// ============================================================================

// Create a module loader
FLOW_FFI_EXPORT FlowModuleHandle flow_module_create(FlowNodeFactoryHandle factory);

// Destroy a module
FLOW_FFI_EXPORT void flow_module_destroy(FlowModuleHandle module);

// Load a module
FLOW_FFI_EXPORT FlowError flow_module_load(FlowModuleHandle module, const char* path);

// Unload a module
FLOW_FFI_EXPORT FlowError flow_module_unload(FlowModuleHandle module);

// Register module nodes
FLOW_FFI_EXPORT FlowError flow_module_register_nodes(FlowModuleHandle module);

// Unregister module nodes
FLOW_FFI_EXPORT FlowError flow_module_unregister_nodes(FlowModuleHandle module);

// Check if module is loaded
FLOW_FFI_EXPORT bool flow_module_is_loaded(FlowModuleHandle module);

// Get module metadata
FLOW_FFI_EXPORT const char* flow_module_get_name(FlowModuleHandle module);
FLOW_FFI_EXPORT const char* flow_module_get_version(FlowModuleHandle module);
FLOW_FFI_EXPORT const char* flow_module_get_author(FlowModuleHandle module);
FLOW_FFI_EXPORT const char* flow_module_get_description(FlowModuleHandle module);

// ============================================================================
// Data Type Management
// ============================================================================

// Create typed data
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_int(int32_t value);
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_double(double value);
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_bool(bool value);
FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_string(const char* value);

// Get typed data
FLOW_FFI_EXPORT FlowError flow_data_get_int(FlowNodeDataHandle data, int32_t* value);
FLOW_FFI_EXPORT FlowError flow_data_get_double(FlowNodeDataHandle data, double* value);
FLOW_FFI_EXPORT FlowError flow_data_get_bool(FlowNodeDataHandle data, bool* value);
FLOW_FFI_EXPORT FlowError flow_data_get_string(FlowNodeDataHandle data, char** value);

// Get data type
FLOW_FFI_EXPORT const char* flow_data_get_type(FlowNodeDataHandle data);

// Destroy data
FLOW_FFI_EXPORT void flow_data_destroy(FlowNodeDataHandle data);

// Convert data to string representation
FLOW_FFI_EXPORT const char* flow_data_to_string(FlowNodeDataHandle data);

// ============================================================================
// Memory Management Helpers
// ============================================================================

// Free strings allocated by the library
FLOW_FFI_EXPORT void flow_free_string(char* str);

// Free string arrays
FLOW_FFI_EXPORT void flow_free_string_array(char** array, size_t count);

// Free handle arrays
FLOW_FFI_EXPORT void flow_free_handle_array(void** array);

// Free connection info arrays
FLOW_FFI_EXPORT void flow_free_connection_array(FlowConnectionInfo* connections, size_t count);

// ============================================================================
// Event Callbacks (Phase 5)
// ============================================================================

// Callback function types
typedef void (*FlowNodeEventCallback)(FlowNodeHandle node, void* user_data);
typedef void (*FlowConnectionEventCallback)(FlowConnectionHandle conn, void* user_data);
typedef void (*FlowErrorEventCallback)(const char* error, void* user_data);
typedef void (*FlowNodeDataEventCallback)(FlowNodeHandle node, const char* port_key,
                                          FlowNodeDataHandle data, void* user_data);

// Event registration handle for tracking callbacks
typedef struct FlowEventRegistration* FlowEventRegistrationHandle;

// Graph Events
FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_node_added(FlowGraphHandle graph,
                                                                     FlowNodeEventCallback callback,
                                                                     void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle
flow_graph_on_node_removed(FlowGraphHandle graph, FlowNodeEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_nodes_connected(
    FlowGraphHandle graph, FlowConnectionEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_nodes_disconnected(
    FlowGraphHandle graph, FlowConnectionEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_graph_on_error(FlowGraphHandle graph,
                                                                FlowErrorEventCallback callback,
                                                                void* user_data);

// Node Events
FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_node_on_compute(FlowNodeHandle node,
                                                                 FlowNodeEventCallback callback,
                                                                 void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle flow_node_on_error(FlowNodeHandle node,
                                                               FlowErrorEventCallback callback,
                                                               void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle
flow_node_on_set_input(FlowNodeHandle node, FlowNodeDataEventCallback callback, void* user_data);

FLOW_FFI_EXPORT FlowEventRegistrationHandle
flow_node_on_set_output(FlowNodeHandle node, FlowNodeDataEventCallback callback, void* user_data);

// Event unregistration
FLOW_FFI_EXPORT FlowError flow_event_unregister(FlowEventRegistrationHandle registration);

// Event registration validation
FLOW_FFI_EXPORT bool flow_event_is_valid(FlowEventRegistrationHandle registration);

#ifdef __cplusplus
}
#endif

#endif // FLOW_FFI_H