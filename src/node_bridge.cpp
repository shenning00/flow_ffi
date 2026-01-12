#include "flow_ffi.h"

#include <flow/core/Node.hpp>
#include <flow/core/NodeData.hpp>

#include <cstring>

#include "error_handling.hpp"
#include "handle_manager.hpp"
#include <nlohmann/json.hpp>

using namespace flow;

// Forward declarations for wrappers (defined in other files)
struct NodeWrapper {
    SharedNode node;
    NodeWrapper(SharedNode n) : node(std::move(n)) {}
};

struct NodeDataWrapper {
    SharedNodeData data;
    NodeDataWrapper(SharedNodeData d) : data(std::move(d)) {}
};

extern "C" {

// ============================================================================
// Node Property Access
// ============================================================================

FLOW_FFI_EXPORT const char* flow_node_get_id(FlowNodeHandle node) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        std::string id_str = std::string(node_wrapper->node->ID());

        // Allocate string that will be freed by flow_free_string
        char* result = new char[id_str.length() + 1];
        std::strcpy(result, id_str.c_str());
        return result;
    });
}

FLOW_FFI_EXPORT const char* flow_node_get_name(FlowNodeHandle node) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        const std::string& name = node_wrapper->node->GetName();

        // Allocate string that will be freed by flow_free_string
        char* result = new char[name.length() + 1];
        std::strcpy(result, name.c_str());
        return result;
    });
}

FLOW_FFI_EXPORT const char* flow_node_get_class(FlowNodeHandle node) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        const std::string& class_name = node_wrapper->node->GetClass();

        // Allocate string that will be freed by flow_free_string
        char* result = new char[class_name.length() + 1];
        std::strcpy(result, class_name.c_str());
        return result;
    });
}

FLOW_FFI_EXPORT FlowError flow_node_set_name(FlowNodeHandle node, const char* name) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_string(name, "name")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        node_wrapper->node->SetName(name);
        return FLOW_SUCCESS;
    });
}

// ============================================================================
// Node Data Operations
// ============================================================================

FLOW_FFI_EXPORT FlowError flow_node_set_input_data(FlowNodeHandle node, const char* port_key,
                                                   FlowNodeDataHandle data) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_string(port_key, "port_key")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        if (!flow_ffi::validate_handle(data, "data")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!data_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            IndexableName key(port_key);
            node_wrapper->node->SetInputData(key, data_wrapper->data, false); // Don't auto-compute
            return FLOW_SUCCESS;
        } catch (const std::out_of_range&) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND, std::string("Input port not found: ") + port_key);
            return FLOW_ERROR_PORT_NOT_FOUND;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to set input data: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowNodeDataHandle flow_node_get_input_data(FlowNodeHandle node,
                                                            const char* port_key) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node")) {
            return nullptr;
        }
        if (!flow_ffi::validate_string(port_key, "port_key")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        try {
            IndexableName key(port_key);
            const SharedNodeData& data = node_wrapper->node->GetInputData(key);

            if (!data) {
                return nullptr; // No data in port
            }

            // Create wrapper and handle
            auto wrapper = NodeDataWrapper(data);
            return reinterpret_cast<FlowNodeDataHandle>(
                flow_ffi::create_handle<NodeDataWrapper>(wrapper));

        } catch (const std::out_of_range&) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND, std::string("Input port not found: ") + port_key);
            return nullptr;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get input data: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT FlowNodeDataHandle flow_node_get_output_data(FlowNodeHandle node,
                                                             const char* port_key) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node")) {
            return nullptr;
        }
        if (!flow_ffi::validate_string(port_key, "port_key")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        try {
            IndexableName key(port_key);
            const SharedNodeData& data = node_wrapper->node->GetOutputData(key);

            if (!data) {
                return nullptr; // No data in port
            }

            // Create wrapper and handle
            auto wrapper = NodeDataWrapper(data);
            return reinterpret_cast<FlowNodeDataHandle>(
                flow_ffi::create_handle<NodeDataWrapper>(wrapper));

        } catch (const std::out_of_range&) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND, std::string("Output port not found: ") + port_key);
            return nullptr;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get output data: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_node_clear_input_data(FlowNodeHandle node, const char* port_key) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_string(port_key, "port_key")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            IndexableName key(port_key);
            node_wrapper->node->SetInputData(key, nullptr, false); // Clear with null data
            return FLOW_SUCCESS;
        } catch (const std::out_of_range&) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND, std::string("Input port not found: ") + port_key);
            return FLOW_ERROR_PORT_NOT_FOUND;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to clear input data: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_node_clear_output_data(FlowNodeHandle node, const char* port_key) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_string(port_key, "port_key")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            IndexableName key(port_key);
            node_wrapper->node->SetOutputData(key, nullptr, false); // Clear with null data
            return FLOW_SUCCESS;
        } catch (const std::out_of_range&) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND, std::string("Output port not found: ") + port_key);
            return FLOW_ERROR_PORT_NOT_FOUND;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to clear output data: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

// ============================================================================
// Node Computation
// ============================================================================

FLOW_FFI_EXPORT FlowError flow_node_invoke_compute(FlowNodeHandle node) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            node_wrapper->node->InvokeCompute();
            return FLOW_SUCCESS;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_COMPUTATION_FAILED, std::string("Node computation failed: ") + e.what());
            return FLOW_ERROR_COMPUTATION_FAILED;
        }
    });
}

FLOW_FFI_EXPORT bool flow_node_validate_required_inputs(FlowNodeHandle node) {
    if (!flow_ffi::validate_handle(node, "node")) {
        return false;
    }

    auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
    if (!node_wrapper) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                     "Invalid node handle");
        return false;
    }

    try {
        // Check if all input ports have data
        const auto& input_ports = node_wrapper->node->GetInputPorts();
        for (const auto& [key, port] : input_ports) {
            try {
                const SharedNodeData& data = node_wrapper->node->GetInputData(key);
                if (!data) {
                    return false; // Missing required input
                }
            } catch (const std::out_of_range&) {
                return false; // Port exists but has no data
            }
        }
        return true;
    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(
            FLOW_ERROR_UNKNOWN, std::string("Failed to validate inputs: ") + e.what());
        return false;
    }
}

// ============================================================================
// Node Connection Status
// ============================================================================

FLOW_FFI_EXPORT bool flow_node_has_connected_inputs(FlowNodeHandle node) {
    if (!flow_ffi::validate_handle(node, "node")) {
        return false;
    }

    auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
    if (!node_wrapper) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                     "Invalid node handle");
        return false;
    }

    try {
        const auto& input_ports = node_wrapper->node->GetInputPorts();

        // Check if any input port has data (indicating a connection)
        for (const auto& [key, port] : input_ports) {
            try {
                const SharedNodeData& data = node_wrapper->node->GetInputData(key);
                if (data) {
                    return true; // Found connected input
                }
            } catch (const std::out_of_range&) {
                // Port exists but has no data - continue checking
            }
        }
        return false;
    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(
            FLOW_ERROR_UNKNOWN, std::string("Failed to check input connections: ") + e.what());
        return false;
    }
}

FLOW_FFI_EXPORT bool flow_node_has_connected_outputs(FlowNodeHandle node) {
    if (!flow_ffi::validate_handle(node, "node")) {
        return false;
    }

    auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
    if (!node_wrapper) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                     "Invalid node handle");
        return false;
    }

    try {
        const auto& output_ports = node_wrapper->node->GetOutputPorts();

        // Check if any output port has data
        for (const auto& [key, port] : output_ports) {
            try {
                const SharedNodeData& data = node_wrapper->node->GetOutputData(key);
                if (data) {
                    return true; // Found output with data
                }
            } catch (const std::out_of_range&) {
                // Port exists but has no data - continue checking
            }
        }
        return false;
    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(
            FLOW_ERROR_UNKNOWN, std::string("Failed to check output connections: ") + e.what());
        return false;
    }
}

// ============================================================================
// Node Serialization
// ============================================================================

FLOW_FFI_EXPORT char* flow_node_save_to_json(FlowNodeHandle node) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        try {
            json j = node_wrapper->node->Save();
            std::string json_str = j.dump();

            // Allocate string that will be freed by flow_free_string
            char* result = new char[json_str.length() + 1];
            std::strcpy(result, json_str.c_str());
            return result;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to serialize node: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_node_load_from_json(FlowNodeHandle node, const char* json_str) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_string(json_str, "json_str")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            json j = json::parse(json_str);
            node_wrapper->node->Restore(j);
            return FLOW_SUCCESS;

        } catch (const json::parse_error& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, std::string("JSON parse error: ") + e.what());
            return FLOW_ERROR_INVALID_ARGUMENT;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to deserialize node: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

// ============================================================================
// Port Introspection
// ============================================================================

FLOW_FFI_EXPORT FlowError flow_node_get_input_port_keys(FlowNodeHandle node, char*** port_keys,
                                                        size_t* count) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node") ||
            !flow_ffi::validate_pointer(port_keys, "port_keys") ||
            !flow_ffi::validate_pointer(count, "count")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            const auto& input_ports = node_wrapper->node->GetInputPorts();
            *count = input_ports.size();

            if (*count == 0) {
                *port_keys = nullptr;
                return FLOW_SUCCESS;
            }

            // Allocate array of char* pointers
            *port_keys = new char*[*count];

            size_t i = 0;
            for (const auto& [key, port] : input_ports) {
                std::string key_str = std::string(key);
                (*port_keys)[i] = new char[key_str.length() + 1];
                std::strcpy((*port_keys)[i], key_str.c_str());
                i++;
            }

            return FLOW_SUCCESS;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get input port keys: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_node_get_output_port_keys(FlowNodeHandle node, char*** port_keys,
                                                         size_t* count) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node") ||
            !flow_ffi::validate_pointer(port_keys, "port_keys") ||
            !flow_ffi::validate_pointer(count, "count")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            const auto& output_ports = node_wrapper->node->GetOutputPorts();
            *count = output_ports.size();

            if (*count == 0) {
                *port_keys = nullptr;
                return FLOW_SUCCESS;
            }

            // Allocate array of char* pointers
            *port_keys = new char*[*count];

            size_t i = 0;
            for (const auto& [key, port] : output_ports) {
                std::string key_str = std::string(key);
                (*port_keys)[i] = new char[key_str.length() + 1];
                std::strcpy((*port_keys)[i], key_str.c_str());
                i++;
            }

            return FLOW_SUCCESS;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get output port keys: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

// ============================================================================
// Port Type and Description APIs
// ============================================================================

FLOW_FFI_EXPORT const char* flow_node_get_input_port_type(FlowNodeHandle node,
                                                          const char* port_key) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node") ||
            !flow_ffi::validate_pointer(const_cast<void*>(static_cast<const void*>(port_key)),
                                        "port_key")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        try {
            IndexableName port_name(port_key);
            auto input_port = node_wrapper->node->GetInputPort(port_name);
            std::string type_str = std::string(input_port->GetDataType());

            // Allocate and copy string - caller must free with flow_free_string
            char* result = new char[type_str.length() + 1];
            std::strcpy(result, type_str.c_str());

            flow_ffi::ErrorManager::instance().clear_error();
            return result;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND,
                std::string("Failed to get input port type: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT const char* flow_node_get_output_port_type(FlowNodeHandle node,
                                                           const char* port_key) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node") ||
            !flow_ffi::validate_pointer(const_cast<void*>(static_cast<const void*>(port_key)),
                                        "port_key")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        try {
            IndexableName port_name(port_key);
            auto output_port = node_wrapper->node->GetOutputPort(port_name);
            std::string type_str = std::string(output_port->GetDataType());

            // Allocate and copy string - caller must free with flow_free_string
            char* result = new char[type_str.length() + 1];
            std::strcpy(result, type_str.c_str());

            flow_ffi::ErrorManager::instance().clear_error();
            return result;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND,
                std::string("Failed to get output port type: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT const char*
flow_node_get_port_description(FlowNodeHandle node, const char* port_key, bool is_input_port) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(node, "node") ||
            !flow_ffi::validate_pointer(const_cast<void*>(static_cast<const void*>(port_key)),
                                        "port_key")) {
            return nullptr;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return nullptr;
        }

        try {
            IndexableName port_name(port_key);
            std::string caption_str;

            if (is_input_port) {
                auto input_port = node_wrapper->node->GetInputPort(port_name);
                caption_str = std::string(input_port->GetCaption());
            } else {
                auto output_port = node_wrapper->node->GetOutputPort(port_name);
                caption_str = std::string(output_port->GetCaption());
            }

            // Allocate and copy string - caller must free with flow_free_string
            char* result = new char[caption_str.length() + 1];
            std::strcpy(result, caption_str.c_str());

            flow_ffi::ErrorManager::instance().clear_error();
            return result;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_PORT_NOT_FOUND,
                std::string("Failed to get port description: ") + e.what());
            return nullptr;
        }
    });
}

// ============================================================================
// Port Metadata (for Optional Values)
// ============================================================================

namespace {
// Helper function to map flow-core type strings to interworking types
std::string MapTypeToInterworkingType(std::string_view flow_type) {
    // Check for common primitive types
    if (flow_type == "int" || flow_type == "int32_t" || flow_type == "int64_t" ||
        flow_type == "uint32_t" || flow_type == "uint64_t" || flow_type == "size_t") {
        return "integer";
    } else if (flow_type == "float" || flow_type == "double") {
        return "float";
    } else if (flow_type == "bool") {
        return "boolean";
    } else if (flow_type == "std::string" || flow_type == "string" || flow_type == "const char*") {
        return "string";
    }

    // Default to "none" for complex types
    return "none";
}

// Helper function to create interworking JSON for a port
std::string CreateInterworkingJson(const SharedPort& port) {
    json j;

    // Get the type and map it
    std::string_view data_type = port->GetDataType();
    std::string interworking_type = MapTypeToInterworkingType(data_type);

    j["type"] = interworking_type;

    // If port has default data and it's an editable type, include the value
    if (interworking_type != "none" && port->GetData()) {
        try {
            std::string value_str = port->GetData()->ToString();
            j["value"] = value_str;
        } catch (...) {
            // If ToString fails, just include type without value
        }
    }

    return j.dump();
}
} // namespace

FLOW_FFI_EXPORT FlowError flow_node_get_port_metadata(FlowNodeHandle node, const char* port_key,
                                                      FlowPortMetadata* metadata) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_string(port_key, "port_key")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        if (!flow_ffi::validate_pointer(metadata, "metadata")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            IndexableName key(port_key);

            // Try to get input port first
            SharedPort port = nullptr;
            try {
                port = node_wrapper->node->GetInputPort(key);
            } catch (const std::out_of_range&) {
                // Not an input port, try output
                try {
                    port = node_wrapper->node->GetOutputPort(key);
                } catch (const std::out_of_range&) {
                    flow_ffi::ErrorManager::instance().set_error(
                        FLOW_ERROR_PORT_NOT_FOUND, std::string("Port not found: ") + port_key);
                    return FLOW_ERROR_PORT_NOT_FOUND;
                }
            }

            if (!port) {
                flow_ffi::ErrorManager::instance().set_error(
                    FLOW_ERROR_PORT_NOT_FOUND, std::string("Port not found: ") + port_key);
                return FLOW_ERROR_PORT_NOT_FOUND;
            }

            // Allocate and copy the port key
            char* key_copy = new char[std::strlen(port_key) + 1];
            std::strcpy(key_copy, port_key);
            metadata->key = key_copy;

            // Create interworking JSON
            std::string json_str = CreateInterworkingJson(port);
            char* json_copy = new char[json_str.length() + 1];
            std::strcpy(json_copy, json_str.c_str());
            metadata->interworking_value_json = json_copy;

            // Check if port has default data
            metadata->has_default = (port->GetData() != nullptr);

            return FLOW_SUCCESS;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get port metadata: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_node_get_input_ports_metadata(FlowNodeHandle node,
                                                             FlowPortMetadata** metadata_array,
                                                             size_t* count) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_pointer(metadata_array, "metadata_array")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        if (!flow_ffi::validate_pointer(count, "count")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
        if (!node_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            const auto& input_ports = node_wrapper->node->GetInputPorts();
            *count = input_ports.size();

            if (*count == 0) {
                *metadata_array = nullptr;
                return FLOW_SUCCESS;
            }

            // Allocate array of metadata structures
            *metadata_array = new FlowPortMetadata[*count];

            size_t i = 0;
            for (const auto& [key, port] : input_ports) {
                // Allocate and copy the port key
                std::string key_str = std::string(key);
                char* key_copy = new char[key_str.length() + 1];
                std::strcpy(key_copy, key_str.c_str());
                (*metadata_array)[i].key = key_copy;

                // Create interworking JSON
                std::string json_str = CreateInterworkingJson(port);
                char* json_copy = new char[json_str.length() + 1];
                std::strcpy(json_copy, json_str.c_str());
                (*metadata_array)[i].interworking_value_json = json_copy;

                // Check if port has default data
                (*metadata_array)[i].has_default = (port->GetData() != nullptr);

                i++;
            }

            return FLOW_SUCCESS;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get input ports metadata: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT void flow_free_port_metadata_array(FlowPortMetadata* metadata_array, size_t count) {
    if (metadata_array == nullptr) {
        return;
    }

    // Free each metadata entry's strings
    for (size_t i = 0; i < count; i++) {
        if (metadata_array[i].key != nullptr) {
            delete[] metadata_array[i].key;
        }
        if (metadata_array[i].interworking_value_json != nullptr) {
            delete[] metadata_array[i].interworking_value_json;
        }
    }

    // Free the array itself
    delete[] metadata_array;
}

FLOW_FFI_EXPORT void flow_free_port_metadata(FlowPortMetadata* metadata) {
    if (metadata == nullptr) {
        return;
    }

    // Free the strings within the metadata structure
    if (metadata->key != nullptr) {
        delete[] metadata->key;
        metadata->key = nullptr;
    }
    if (metadata->interworking_value_json != nullptr) {
        delete[] metadata->interworking_value_json;
        metadata->interworking_value_json = nullptr;
    }

    // Note: We don't delete the metadata structure itself
    // because it's typically allocated on the stack by the caller
}

} // extern "C"