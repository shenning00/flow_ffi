#include "flow_ffi.h"

#include "error_handling.hpp"
#include "handle_manager.hpp"

// Placeholder implementations - will be completed in subsequent phases

extern "C" {

// ============================================================================
// Memory Management Helpers
// ============================================================================

FLOW_FFI_EXPORT void flow_free_string(char* str) {
    if (str) {
        delete[] str;
    }
}

FLOW_FFI_EXPORT void flow_free_string_array(char** array, size_t count) {
    if (array) {
        for (size_t i = 0; i < count; ++i) {
            flow_free_string(array[i]);
        }
        delete[] array;
    }
}

FLOW_FFI_EXPORT void flow_free_handle_array(void** array) {
    if (array) {
        delete[] array;
    }
}

FLOW_FFI_EXPORT void flow_free_connection_array(FlowConnectionInfo* connections, size_t count) {
    if (connections) {
        for (size_t i = 0; i < count; ++i) {
            flow_free_string(const_cast<char*>(connections[i].id));
            flow_free_string(const_cast<char*>(connections[i].source_node_id));
            flow_free_string(const_cast<char*>(connections[i].source_port_key));
            flow_free_string(const_cast<char*>(connections[i].target_node_id));
            flow_free_string(const_cast<char*>(connections[i].target_port_key));
        }
        delete[] connections;
    }
}

// ============================================================================
// Placeholder implementations (to be completed in subsequent phases)
// ============================================================================

// Environment Management functions are now implemented in env_bridge.cpp

// Graph Management functions are now implemented in graph_bridge.cpp
// Connection Management functions are now implemented in connection_bridge.cpp

// Node Management functions are now implemented in node_bridge.cpp

// Additional placeholder implementations for other functions...
// (All other functions return FLOW_ERROR_NOT_IMPLEMENTED for now)

} // extern "C"