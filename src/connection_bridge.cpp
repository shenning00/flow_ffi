// Connection bridge implementation - Phase 4
// Bridges the flow-core Connection class to the C FFI interface

#include "flow_ffi.h"

#include "error_handling.hpp"
#include "handle_manager.hpp"

// Include flow-core headers
#include <flow/core/Connection.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/UUID.hpp>

#include <string>

using namespace flow;

extern "C" {

// ============================================================================
// Connection Management
// ============================================================================

FLOW_FFI_EXPORT const char* flow_connection_get_id(FlowConnectionHandle conn) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(conn, "conn")) {
            return nullptr;
        }

        auto* conn_ptr = flow_ffi::get_handle<SharedConnection>(conn);
        if (!conn_ptr || !*conn_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get connection from handle");
            return nullptr;
        }

        // Convert UUID to string and return as static string
        static thread_local std::string id_str;
        id_str = static_cast<std::string>((*conn_ptr)->ID());

        flow_ffi::ErrorManager::instance().clear_error();
        return id_str.c_str();
    });
}

FLOW_FFI_EXPORT const char* flow_connection_get_start_node_id(FlowConnectionHandle conn) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(conn, "conn")) {
            return nullptr;
        }

        auto* conn_ptr = flow_ffi::get_handle<SharedConnection>(conn);
        if (!conn_ptr || !*conn_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get connection from handle");
            return nullptr;
        }

        // Convert UUID to string and return as static string
        static thread_local std::string start_id_str;
        start_id_str = static_cast<std::string>((*conn_ptr)->StartNodeID());

        flow_ffi::ErrorManager::instance().clear_error();
        return start_id_str.c_str();
    });
}

FLOW_FFI_EXPORT const char* flow_connection_get_start_port(FlowConnectionHandle conn) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(conn, "conn")) {
            return nullptr;
        }

        auto* conn_ptr = flow_ffi::get_handle<SharedConnection>(conn);
        if (!conn_ptr || !*conn_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get connection from handle");
            return nullptr;
        }

        // Convert IndexableName to string and return as static string
        static thread_local std::string start_port_str;
        start_port_str = std::string((*conn_ptr)->StartPortKey().name());

        flow_ffi::ErrorManager::instance().clear_error();
        return start_port_str.c_str();
    });
}

FLOW_FFI_EXPORT const char* flow_connection_get_end_node_id(FlowConnectionHandle conn) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(conn, "conn")) {
            return nullptr;
        }

        auto* conn_ptr = flow_ffi::get_handle<SharedConnection>(conn);
        if (!conn_ptr || !*conn_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get connection from handle");
            return nullptr;
        }

        // Convert UUID to string and return as static string
        static thread_local std::string end_id_str;
        end_id_str = static_cast<std::string>((*conn_ptr)->EndNodeID());

        flow_ffi::ErrorManager::instance().clear_error();
        return end_id_str.c_str();
    });
}

FLOW_FFI_EXPORT const char* flow_connection_get_end_port(FlowConnectionHandle conn) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(conn, "conn")) {
            return nullptr;
        }

        auto* conn_ptr = flow_ffi::get_handle<SharedConnection>(conn);
        if (!conn_ptr || !*conn_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Failed to get connection from handle");
            return nullptr;
        }

        // Convert IndexableName to string and return as static string
        static thread_local std::string end_port_str;
        end_port_str = std::string((*conn_ptr)->EndPortKey().name());

        flow_ffi::ErrorManager::instance().clear_error();
        return end_port_str.c_str();
    });
}

} // extern "C"