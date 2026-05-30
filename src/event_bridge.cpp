// Event bridge implementation - Phase 5
// Bridges the flow-core event system to the C FFI interface

#include "flow_ffi.h"

#include "error_handling.hpp"
#include "handle_manager.hpp"
#include "node_data_wrapper.hpp"
#include "node_wrapper.hpp"

// Include flow-core headers
#include <flow/core/Connection.hpp>
#include <flow/core/Graph.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace flow;

namespace
{

// Heap-duplicate a C-string using new char[] so it is compatible with
// flow_free_string (which calls delete[]).  Ownership transfers to the caller.
// Returns nullptr when s is nullptr.
char* dup_cstr(const char* s)
{
    if (!s) return nullptr;
    const std::size_t len = std::strlen(s);
    char* buf             = new char[len + 1];
    std::memcpy(buf, s, len + 1);
    return buf;
}

// Overload for std::string_view (e.g. IndexableName::name() return type).
char* dup_cstr(std::string_view sv)
{
    char* buf = new char[sv.size() + 1];
    std::memcpy(buf, sv.data(), sv.size());
    buf[sv.size()] = '\0';
    return buf;
}

} // namespace

// Event registration structure
struct FlowEventRegistration
{
    enum class Type
    {
        GraphNodeAdded,
        GraphNodeRemoved,
        GraphNodesConnected,
        GraphNodesDisconnected,
        GraphError,
        NodeCompute,
        NodeError,
        NodeSetInput,
        NodeSetOutput
    } type;

    void* handle;           // Graph or Node handle
    void* callback;         // Callback function pointer
    void* user_data;        // User data
    IndexableName event_id; // ID used for unregistering from EventDispatcher

    FlowEventRegistration(Type t, void* h, void* cb, void* ud, const IndexableName& id)
        : type(t), handle(h), callback(cb), user_data(ud), event_id(id)
    {
    }
};

// Global event registration tracking
static std::unordered_set<std::unique_ptr<FlowEventRegistration>> g_event_registrations;
static std::mutex g_events_mutex;

// Helper to generate unique event IDs
static IndexableName generate_event_id()
{
    static std::atomic<uint64_t> counter{0};
    return IndexableName{"event_" + std::to_string(counter.fetch_add(1))};
}

// Helper to add event registration tracking
static FlowEventRegistrationHandle add_event_registration(FlowEventRegistration::Type type, void* handle,
                                                          void* callback, void* user_data)
{
    std::lock_guard<std::mutex> lock(g_events_mutex);
    auto event_id     = generate_event_id();
    auto registration = std::make_unique<FlowEventRegistration>(type, handle, callback, user_data, event_id);
    auto* ptr         = registration.get();
    g_event_registrations.insert(std::move(registration));
    return reinterpret_cast<FlowEventRegistrationHandle>(ptr);
}

// Helper to remove event registration
static bool remove_event_registration(FlowEventRegistrationHandle registration)
{
    std::lock_guard<std::mutex> lock(g_events_mutex);
    auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

    auto it = std::find_if(g_event_registrations.begin(), g_event_registrations.end(),
                           [reg](const std::unique_ptr<FlowEventRegistration>& ptr) { return ptr.get() == reg; });

    if (it != g_event_registrations.end())
    {
        g_event_registrations.erase(it);
        return true;
    }
    return false;
}

// Graph Event Implementations
extern "C"
{

    FlowEventRegistrationHandle flow_graph_on_node_added(FlowGraphHandle graph, FlowNodeEventCallback callback,
                                                         void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(graph, "graph") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid graph handle or callback");
                return nullptr;
            }

            auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
            if (!graph_ptr || !*graph_ptr)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get graph from handle");
                return nullptr;
            }

            auto registration =
                add_event_registration(FlowEventRegistration::Type::GraphNodeAdded, graph, (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the graph's OnNodeAdded event
            (*graph_ptr)->OnNodeAdded.Bind(reg->event_id, [callback, user_data](const SharedNode& node) {
                auto node_handle = flow_ffi::get_or_create_node_handle(node);
                callback(static_cast<FlowNodeHandle>(node_handle), user_data);
            });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    FlowEventRegistrationHandle flow_graph_on_node_removed(FlowGraphHandle graph, FlowNodeEventCallback callback,
                                                           void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(graph, "graph") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid graph handle or callback");
                return nullptr;
            }

            auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
            if (!graph_ptr || !*graph_ptr)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get graph from handle");
                return nullptr;
            }

            auto registration = add_event_registration(FlowEventRegistration::Type::GraphNodeRemoved, graph,
                                                       (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the graph's OnNodeRemoved event
            (*graph_ptr)->OnNodeRemoved.Bind(reg->event_id, [callback, user_data](const SharedNode& node) {
                auto node_handle = flow_ffi::get_or_create_node_handle(node);
                callback(static_cast<FlowNodeHandle>(node_handle), user_data);
            });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    FlowEventRegistrationHandle flow_graph_on_nodes_connected(FlowGraphHandle graph,
                                                              FlowConnectionEventCallback callback, void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(graph, "graph") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid graph handle or callback");
                return nullptr;
            }

            auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
            if (!graph_ptr || !*graph_ptr)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get graph from handle");
                return nullptr;
            }

            auto registration = add_event_registration(FlowEventRegistration::Type::GraphNodesConnected, graph,
                                                       (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the graph's OnNodesConnected event
            (*graph_ptr)->OnNodesConnected.Bind(reg->event_id, [callback, user_data](const SharedConnection& conn) {
                // Convert SharedConnection to handle and call Dart callback
                auto conn_handle = flow_ffi::create_handle<std::shared_ptr<Connection>>(conn);
                callback(static_cast<FlowConnectionHandle>(conn_handle), user_data);
            });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    FlowEventRegistrationHandle flow_graph_on_nodes_disconnected(FlowGraphHandle graph,
                                                                 FlowConnectionEventCallback callback, void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(graph, "graph") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid graph handle or callback");
                return nullptr;
            }

            auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
            if (!graph_ptr || !*graph_ptr)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get graph from handle");
                return nullptr;
            }

            auto registration = add_event_registration(FlowEventRegistration::Type::GraphNodesDisconnected, graph,
                                                       (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the graph's OnNodesDisconnected event
            (*graph_ptr)->OnNodesDisconnected.Bind(reg->event_id, [callback, user_data](const SharedConnection& conn) {
                // Convert SharedConnection to handle and call Dart callback
                auto conn_handle = flow_ffi::create_handle<std::shared_ptr<Connection>>(conn);
                callback(static_cast<FlowConnectionHandle>(conn_handle), user_data);
            });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    FlowEventRegistrationHandle flow_graph_on_error(FlowGraphHandle graph, FlowErrorEventCallback callback,
                                                    void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(graph, "graph") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid graph handle or callback");
                return nullptr;
            }

            auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(graph);
            if (!graph_ptr || !*graph_ptr)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get graph from handle");
                return nullptr;
            }

            auto registration =
                add_event_registration(FlowEventRegistration::Type::GraphError, graph, (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the graph's OnError event
            (*graph_ptr)->OnError.Bind(reg->event_id, [callback, user_data](const std::exception& error) {
                // Ownership transferred to caller; Dart frees via flow_free_string.
                callback(dup_cstr(error.what()), user_data);
            });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    // Node Event Implementations
    FlowEventRegistrationHandle flow_node_on_compute(FlowNodeHandle node, FlowNodeEventCallback callback,
                                                     void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(node, "node") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid node handle or callback");
                return nullptr;
            }

            auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
            if (!node_wrapper || !node_wrapper->node)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get node from handle");
                return nullptr;
            }

            auto registration =
                add_event_registration(FlowEventRegistration::Type::NodeCompute, node, (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the node's OnCompute event
            node_wrapper->node->OnCompute.Bind(reg->event_id,
                                               [callback, user_data, node]() { callback(node, user_data); });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    FlowEventRegistrationHandle flow_node_on_error(FlowNodeHandle node, FlowErrorEventCallback callback,
                                                   void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(node, "node") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid node handle or callback");
                return nullptr;
            }

            auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
            if (!node_wrapper || !node_wrapper->node)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get node from handle");
                return nullptr;
            }

            auto registration =
                add_event_registration(FlowEventRegistration::Type::NodeError, node, (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the node's OnError event
            node_wrapper->node->OnError.Bind(reg->event_id, [callback, user_data](const std::exception& error) {
                // Ownership transferred to caller; Dart frees via flow_free_string.
                callback(dup_cstr(error.what()), user_data);
            });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    FlowEventRegistrationHandle flow_node_on_set_input(FlowNodeHandle node, FlowNodeDataEventCallback callback,
                                                       void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(node, "node") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid node handle or callback");
                return nullptr;
            }

            auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
            if (!node_wrapper || !node_wrapper->node)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get node from handle");
                return nullptr;
            }

            auto registration =
                add_event_registration(FlowEventRegistration::Type::NodeSetInput, node, (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the node's OnSetInput event
            node_wrapper->node->OnSetInput.Bind(
                reg->event_id, [callback, user_data, node](const IndexableName& port_key, const SharedNodeData& data) {
                    // Convert data to handle
                    // Wrap the SharedNodeData in a NodeDataWrapper so downstream
                    // accessors like flow_data_is_image / flow_data_image_borrow
                    // (and the existing flow_data_get_* family) succeed on this
                    // handle.  The rest of the FFI surface treats NodeDataWrapper
                    // as the canonical handle wrapper; using a raw SharedNodeData
                    // here previously caused get_handle<NodeDataWrapper> to
                    // dynamic_cast to null, masking real images as "not image"
                    // when they crossed the event boundary.
                    auto data_handle = flow_ffi::create_handle<NodeDataWrapper>(NodeDataWrapper(data));
                    // Ownership transferred to caller; Dart frees via flow_free_string.
                    callback(node, dup_cstr(port_key.name()), static_cast<FlowNodeDataHandle>(data_handle), user_data);
                });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    FlowEventRegistrationHandle flow_node_on_set_output(FlowNodeHandle node, FlowNodeDataEventCallback callback,
                                                        void* user_data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(node, "node") || !callback)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid node handle or callback");
                return nullptr;
            }

            auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(node);
            if (!node_wrapper || !node_wrapper->node)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                             "Failed to get node from handle");
                return nullptr;
            }

            auto registration =
                add_event_registration(FlowEventRegistration::Type::NodeSetOutput, node, (void*)callback, user_data);

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Bind to the node's OnSetOutput event
            node_wrapper->node->OnSetOutput.Bind(
                reg->event_id, [callback, user_data, node](const IndexableName& port_key, const SharedNodeData& data) {
                    // Convert data to handle
                    // Wrap the SharedNodeData in a NodeDataWrapper so downstream
                    // accessors like flow_data_is_image / flow_data_image_borrow
                    // (and the existing flow_data_get_* family) succeed on this
                    // handle.  The rest of the FFI surface treats NodeDataWrapper
                    // as the canonical handle wrapper; using a raw SharedNodeData
                    // here previously caused get_handle<NodeDataWrapper> to
                    // dynamic_cast to null, masking real images as "not image"
                    // when they crossed the event boundary.
                    auto data_handle = flow_ffi::create_handle<NodeDataWrapper>(NodeDataWrapper(data));
                    // Ownership transferred to caller; Dart frees via flow_free_string.
                    callback(node, dup_cstr(port_key.name()), static_cast<FlowNodeDataHandle>(data_handle), user_data);
                });

            flow_ffi::ErrorManager::instance().clear_error();
            return registration;
        });
    }

    // Event Management
    FlowError flow_event_unregister(FlowEventRegistrationHandle registration)
    {
        FLOW_API_CALL({
            if (!registration)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                             "Invalid registration handle");
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

            // Unbind from the appropriate event dispatcher
            switch (reg->type)
            {
            case FlowEventRegistration::Type::GraphNodeAdded:
            case FlowEventRegistration::Type::GraphNodeRemoved:
            case FlowEventRegistration::Type::GraphNodesConnected:
            case FlowEventRegistration::Type::GraphNodesDisconnected:
            case FlowEventRegistration::Type::GraphError: {
                auto* graph_ptr = flow_ffi::get_handle<std::shared_ptr<Graph>>(reg->handle);
                if (graph_ptr && *graph_ptr)
                {
                    switch (reg->type)
                    {
                    case FlowEventRegistration::Type::GraphNodeAdded:
                        (*graph_ptr)->OnNodeAdded.Unbind(reg->event_id);
                        break;
                    case FlowEventRegistration::Type::GraphNodeRemoved:
                        (*graph_ptr)->OnNodeRemoved.Unbind(reg->event_id);
                        break;
                    case FlowEventRegistration::Type::GraphNodesConnected:
                        (*graph_ptr)->OnNodesConnected.Unbind(reg->event_id);
                        break;
                    case FlowEventRegistration::Type::GraphNodesDisconnected:
                        (*graph_ptr)->OnNodesDisconnected.Unbind(reg->event_id);
                        break;
                    case FlowEventRegistration::Type::GraphError:
                        (*graph_ptr)->OnError.Unbind(reg->event_id);
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
            case FlowEventRegistration::Type::NodeCompute:
            case FlowEventRegistration::Type::NodeError:
            case FlowEventRegistration::Type::NodeSetInput:
            case FlowEventRegistration::Type::NodeSetOutput: {
                auto* node_wrapper = flow_ffi::get_handle<NodeWrapper>(reg->handle);
                if (node_wrapper && node_wrapper->node)
                {
                    switch (reg->type)
                    {
                    case FlowEventRegistration::Type::NodeCompute:
                        node_wrapper->node->OnCompute.Unbind(reg->event_id);
                        break;
                    case FlowEventRegistration::Type::NodeError:
                        node_wrapper->node->OnError.Unbind(reg->event_id);
                        break;
                    case FlowEventRegistration::Type::NodeSetInput:
                        node_wrapper->node->OnSetInput.Unbind(reg->event_id);
                        break;
                    case FlowEventRegistration::Type::NodeSetOutput:
                        node_wrapper->node->OnSetOutput.Unbind(reg->event_id);
                        break;
                    default:
                        break;
                    }
                }
                break;
            }
            }

            // Remove from tracking
            if (!remove_event_registration(registration))
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Registration not found");
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            flow_ffi::ErrorManager::instance().clear_error();
            return FLOW_SUCCESS;
        });
    }

    bool flow_event_is_valid(FlowEventRegistrationHandle registration)
    {
        if (!registration)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(g_events_mutex);
        auto* reg = reinterpret_cast<FlowEventRegistration*>(registration);

        auto it = std::find_if(g_event_registrations.begin(), g_event_registrations.end(),
                               [reg](const std::unique_ptr<FlowEventRegistration>& ptr) { return ptr.get() == reg; });

        return it != g_event_registrations.end();
    }

} // extern "C"