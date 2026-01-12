// Graph bridge implementation - Phase 4
// Bridges the flow-core Graph class to the C FFI interface

#include "flow_ffi.h"

#include "env_wrapper.hpp"
#include "error_handling.hpp"
#include "handle_manager.hpp"

// Include flow-core headers
#include <flow/core/Connection.hpp>
#include <flow/core/Env.hpp>
#include <flow/core/Graph.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/UUID.hpp>

// Include JSON support
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using namespace flow;

// Wrapper structure for Node (consistent with factory_bridge.cpp)
struct NodeWrapper {
    SharedNode node;
    NodeWrapper(SharedNode n) : node(std::move(n)) {}
};

extern "C" {

// ============================================================================
// Graph Management
// ============================================================================

FLOW_FFI_EXPORT FlowGraphHandle flow_graph_create(FlowEnvHandle env) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(env, "env")) {
            return nullptr;
        }

        // Get the EnvWrapper from the handle
        auto* env_wrapper = flow_ffi::get_handle<EnvWrapper>(env);
        if (!env_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get EnvWrapper from handle");
            return nullptr;
        }

        if (!env_wrapper->env) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "EnvWrapper has null env pointer");
            return nullptr;
        }

        // Create graph with default name and the provided environment
        auto graph = std::make_shared<Graph>("Default Graph", env_wrapper->env);

        // Create handle wrapper
        auto handle = flow_ffi::create_handle<std::shared_ptr<Graph>>(std::move(graph));
        flow_ffi::ErrorManager::instance().clear_error();
        return static_cast<FlowGraphHandle>(handle);
    });
}

FLOW_FFI_EXPORT void flow_graph_destroy(FlowGraphHandle graph) {
    FLOW_API_CALL_VOID({
        if (!flow_ffi::validate_handle(graph, "graph")) {
            return;
        }

        flow_ffi::release_handle(graph);
        flow_ffi::ErrorManager::instance().clear_error();
    });
}

FLOW_FFI_EXPORT FlowNodeHandle flow_graph_add_node(FlowGraphHandle graph, const char* class_id,
                                                   const char* name) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(graph, "graph") ||
            !flow_ffi::validate_string(class_id, "class_id") ||
            !flow_ffi::validate_string(name, "name")) {
            return nullptr;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return nullptr;
        }

        // Get the node factory from the graph's environment
        auto factory = (*graph_ptr)->GetEnv()->GetFactory();
        if (!factory) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_HANDLE, "Failed to get node factory from environment");
            return nullptr;
        }

        // Step 1: Create the node using the factory (proper two-step workflow)
        auto node = factory->CreateNode(class_id, UUID(), name, (*graph_ptr)->GetEnv());
        if (!node) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_NODE_NOT_FOUND,
                std::string("Failed to create node of class: ") + class_id);
            return nullptr;
        }

        // Step 2: Add the pre-created node to the graph (as expected by flow-core)
        (*graph_ptr)->AddNode(node);

        // Verify node was added successfully by checking if we can retrieve it
        auto verifyNode = (*graph_ptr)->GetNode(node->ID());
        if (!verifyNode) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_NODE_NOT_FOUND,
                                                         "Node was not properly added to graph");
            return nullptr;
        }

        // Create wrapper with the original node (same as factory bridge approach)
        // The original node is now managed by the graph, so it's safe to use
        auto wrapper = NodeWrapper(node);
        flow_ffi::ErrorManager::instance().clear_error();
        return reinterpret_cast<FlowNodeHandle>(flow_ffi::create_handle<NodeWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT FlowError flow_graph_remove_node(FlowGraphHandle graph, const char* node_id) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(graph, "graph") ||
            !flow_ffi::validate_string(node_id, "node_id")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        // Parse UUID from string
        UUID uuid;
        try {
            uuid = UUID(node_id);
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, std::string("Invalid UUID format: ") + e.what());
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        // Remove node from graph
        (*graph_ptr)->RemoveNodeByID(uuid);

        flow_ffi::ErrorManager::instance().clear_error();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT FlowNodeHandle flow_graph_get_node(FlowGraphHandle graph, const char* node_id) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(graph, "graph") ||
            !flow_ffi::validate_string(node_id, "node_id")) {
            return nullptr;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return nullptr;
        }

        // Parse UUID from string
        UUID uuid;
        try {
            uuid = UUID(node_id);
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, std::string("Invalid UUID format: ") + e.what());
            return nullptr;
        }

        // Get node from graph
        auto node = (*graph_ptr)->GetNode(uuid);
        if (!node) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_NODE_NOT_FOUND, std::string("Node not found with ID: ") + node_id);
            return nullptr;
        }

        // Create handle for the node (consistent with factory_bridge.cpp)
        auto wrapper = NodeWrapper(std::move(node));
        auto handle = flow_ffi::create_handle<NodeWrapper>(wrapper);
        flow_ffi::ErrorManager::instance().clear_error();
        return static_cast<FlowNodeHandle>(handle);
    });
}

FLOW_FFI_EXPORT FlowError flow_graph_get_nodes(FlowGraphHandle graph, FlowNodeHandle** nodes,
                                               size_t* count) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(graph, "graph") ||
            !flow_ffi::validate_pointer(nodes, "nodes") ||
            !flow_ffi::validate_pointer(count, "count")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        const auto& node_map = (*graph_ptr)->GetNodes();
        *count = node_map.size();

        if (*count == 0) {
            *nodes = nullptr;
            flow_ffi::ErrorManager::instance().clear_error();
            return FLOW_SUCCESS;
        }

        // Allocate array for node handles
        *nodes = new FlowNodeHandle[*count];

        size_t i = 0;
        for (const auto& [uuid, node] : node_map) {
            auto wrapper = NodeWrapper(node);
            auto handle = flow_ffi::create_handle<NodeWrapper>(wrapper);
            (*nodes)[i++] = static_cast<FlowNodeHandle>(handle);
        }

        flow_ffi::ErrorManager::instance().clear_error();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT FlowConnectionHandle flow_graph_connect_nodes(FlowGraphHandle graph,
                                                              const char* source_id,
                                                              const char* source_port,
                                                              const char* target_id,
                                                              const char* target_port) {
    flow_ffi::ErrorSetter error_setter;
    try {
        if (!flow_ffi::validate_handle(graph, "graph") ||
            !flow_ffi::validate_string(source_id, "source_id") ||
            !flow_ffi::validate_string(source_port, "source_port") ||
            !flow_ffi::validate_string(target_id, "target_id") ||
            !flow_ffi::validate_string(target_port, "target_port")) {
            return nullptr;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return nullptr;
        }

        // Parse UUIDs from strings
        UUID source_uuid, target_uuid;
        try {
            source_uuid = UUID(source_id);
            target_uuid = UUID(target_id);
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, std::string("Invalid UUID format: ") + e.what());
            return nullptr;
        }

        // Create connection
        auto connection = (*graph_ptr)
                              ->ConnectNodes(source_uuid, IndexableName(source_port), target_uuid,
                                             IndexableName(target_port));

        if (!connection) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_CONNECTION_FAILED, "Failed to create connection between nodes");
            return nullptr;
        }

        // Create handle for the connection
        auto handle = flow_ffi::create_handle<SharedConnection>(std::move(connection));
        flow_ffi::ErrorManager::instance().clear_error();
        return static_cast<FlowConnectionHandle>(handle);
    } catch (const std::exception& e) {
        error_setter.set_error(FLOW_ERROR_UNKNOWN, e.what());
        return nullptr;
    } catch (...) {
        error_setter.set_error(FLOW_ERROR_UNKNOWN, "Unknown exception occurred");
        return nullptr;
    }
}

FLOW_FFI_EXPORT FlowError flow_graph_disconnect_nodes(FlowGraphHandle graph,
                                                      const char* connection_id) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(graph, "graph") ||
            !flow_ffi::validate_string(connection_id, "connection_id")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        // Parse connection UUID from string
        UUID conn_uuid;
        try {
            conn_uuid = UUID(connection_id);
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, std::string("Invalid UUID format: ") + e.what());
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        // Find the connection in the graph by iterating through all connections
        const auto& connections = (*graph_ptr)->GetConnections();
        SharedConnection connection = nullptr;

        for (const auto& [node_uuid, conn] : connections) {
            if (conn->ID() == conn_uuid) {
                connection = conn;
                break;
            }
        }

        if (!connection) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_CONNECTION_FAILED,
                std::string("Connection not found with ID: ") + connection_id);
            return FLOW_ERROR_CONNECTION_FAILED;
        }

        // Disconnect using the connection details
        (*graph_ptr)
            ->DisconnectNodes(connection->StartNodeID(), connection->StartPortKey(),
                              connection->EndNodeID(), connection->EndPortKey());

        flow_ffi::ErrorManager::instance().clear_error();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT FlowError flow_graph_run(FlowGraphHandle graph) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(graph, "graph")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        // Run the graph
        (*graph_ptr)->Run();

        flow_ffi::ErrorManager::instance().clear_error();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT FlowError flow_graph_clear(FlowGraphHandle graph) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(graph, "graph")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        // Clear all nodes and connections
        (*graph_ptr)->Clear();

        flow_ffi::ErrorManager::instance().clear_error();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT char* flow_graph_save_to_json(FlowGraphHandle graph) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(graph, "graph")) {
            return nullptr;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return nullptr;
        }

        // Serialize graph to JSON
        nlohmann::json j;
        to_json(j, **graph_ptr);
        std::string json_str = j.dump(2);

        // Allocate C string
        char* result = new char[json_str.length() + 1];
        std::strcpy(result, json_str.c_str());

        flow_ffi::ErrorManager::instance().clear_error();
        return result;
    });
}

FLOW_FFI_EXPORT FlowError flow_graph_load_from_json(FlowGraphHandle graph, const char* json_str) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(graph, "graph") ||
            !flow_ffi::validate_string(json_str, "json_str")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            // Parse JSON and restore graph state
            nlohmann::json j = nlohmann::json::parse(json_str);
            from_json(j, **graph_ptr);
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT, std::string("JSON parsing failed: ") + e.what());
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        flow_ffi::ErrorManager::instance().clear_error();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT FlowError flow_graph_get_connections(FlowGraphHandle graph,
                                                     FlowConnectionInfo** connections,
                                                     size_t* count) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(graph, "graph") || !connections || !count) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT,
                "Invalid arguments passed to flow_graph_get_connections");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
        if (!graph_ptr || !*graph_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get graph from handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        const auto& graph_connections = (*graph_ptr)->GetConnections();
        *count = graph_connections.Size();

        if (*count == 0) {
            *connections = nullptr;
            flow_ffi::ErrorManager::instance().clear_error();
            return FLOW_SUCCESS;
        }

        // Allocate array of FlowConnectionInfo structs
        *connections = new FlowConnectionInfo[*count];

        size_t index = 0;
        for (const auto& conn_pair : graph_connections) {
            const auto& conn = conn_pair.second;
            // Convert UUIDs to strings and allocate memory
            auto id_str = std::string(conn->ID());
            auto start_id_str = std::string(conn->StartNodeID());
            auto end_id_str = std::string(conn->EndNodeID());
            auto start_port_str = std::string(conn->StartPortKey());
            auto end_port_str = std::string(conn->EndPortKey());

            // Manually allocate C strings
            (*connections)[index].id = new char[id_str.length() + 1];
            std::strcpy(const_cast<char*>((*connections)[index].id), id_str.c_str());

            (*connections)[index].source_node_id = new char[start_id_str.length() + 1];
            std::strcpy(const_cast<char*>((*connections)[index].source_node_id),
                        start_id_str.c_str());

            (*connections)[index].source_port_key = new char[start_port_str.length() + 1];
            std::strcpy(const_cast<char*>((*connections)[index].source_port_key),
                        start_port_str.c_str());

            (*connections)[index].target_node_id = new char[end_id_str.length() + 1];
            std::strcpy(const_cast<char*>((*connections)[index].target_node_id),
                        end_id_str.c_str());

            (*connections)[index].target_port_key = new char[end_port_str.length() + 1];
            std::strcpy(const_cast<char*>((*connections)[index].target_port_key),
                        end_port_str.c_str());

            index++;
        }

        flow_ffi::ErrorManager::instance().clear_error();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT bool flow_graph_can_connect(FlowGraphHandle graph, const char* source_id,
                                            const char* source_port, const char* target_id,
                                            const char* target_port) {
    if (!flow_ffi::validate_handle(graph, "graph") ||
        !flow_ffi::validate_string(source_id, "source_id") ||
        !flow_ffi::validate_string(source_port, "source_port") ||
        !flow_ffi::validate_string(target_id, "target_id") ||
        !flow_ffi::validate_string(target_port, "target_port")) {
        return false;
    }

    auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
    if (!graph_ptr || !*graph_ptr) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                     "Failed to get graph from handle");
        return false;
    }

    try {
        UUID start_uuid(source_id);
        UUID end_uuid(target_id);
        IndexableName start_key(source_port);
        IndexableName end_key(target_port);

        bool can_connect = (*graph_ptr)->CanConnectNode(start_uuid, start_key, end_uuid, end_key);

        flow_ffi::ErrorManager::instance().clear_error();
        return can_connect;
    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(
            FLOW_ERROR_INVALID_ARGUMENT, std::string("Failed to validate connection: ") + e.what());
        return false;
    }
}

} // extern "C"