#include "flow_ffi.h"

#include <flow/core/NodeData.hpp>

#include <cstring>

#include "error_handling.hpp"
#include "handle_manager.hpp"

using namespace flow;

// Wrapper structure for NodeData (forward declaration in node_bridge.cpp)
struct NodeDataWrapper {
    SharedNodeData data;
    NodeDataWrapper(SharedNodeData d) : data(std::move(d)) {}
};

// Helper function to create typed NodeData
template <typename T>
SharedNodeData CreateTypedData(const T& value) {
    return std::make_shared<detail::NodeData<T>>(value);
}

extern "C" {

// ============================================================================
// Data Creation Functions
// ============================================================================

FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_int(int32_t value) {
    FLOW_API_CALL_HANDLE({
        auto data = CreateTypedData<int>(static_cast<int>(value));
        auto wrapper = NodeDataWrapper(data);
        return reinterpret_cast<FlowNodeDataHandle>(
            flow_ffi::create_handle<NodeDataWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_double(double value) {
    FLOW_API_CALL_HANDLE({
        auto data = CreateTypedData<double>(value);
        auto wrapper = NodeDataWrapper(data);
        return reinterpret_cast<FlowNodeDataHandle>(
            flow_ffi::create_handle<NodeDataWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_bool(bool value) {
    FLOW_API_CALL_HANDLE({
        auto data = CreateTypedData<bool>(value);
        auto wrapper = NodeDataWrapper(data);
        return reinterpret_cast<FlowNodeDataHandle>(
            flow_ffi::create_handle<NodeDataWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_string(const char* value) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_string(value, "value")) {
            return nullptr;
        }

        auto data = CreateTypedData<std::string>(std::string(value));
        auto wrapper = NodeDataWrapper(data);
        return reinterpret_cast<FlowNodeDataHandle>(
            flow_ffi::create_handle<NodeDataWrapper>(wrapper));
    });
}

// ============================================================================
// Data Access Functions
// ============================================================================

FLOW_FFI_EXPORT FlowError flow_data_get_int(FlowNodeDataHandle data, int32_t* value) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(data, "data")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_pointer(value, "value")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!data_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        if (!data_wrapper->data) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Data is null");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        // Check if the data is of the correct type
        if (data_wrapper->data->Type() != TypeName_v<int>) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_TYPE_MISMATCH,
                std::string("Expected int, got ") + std::string(data_wrapper->data->Type()));
            return FLOW_ERROR_TYPE_MISMATCH;
        }

        try {
            auto* typed_data = static_cast<detail::NodeData<int>*>(data_wrapper->data.get());
            *value = static_cast<int32_t>(typed_data->Get());
            return FLOW_SUCCESS;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get int value: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_data_get_double(FlowNodeDataHandle data, double* value) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(data, "data")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_pointer(value, "value")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!data_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        if (!data_wrapper->data) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Data is null");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        // Check if the data is of the correct type
        if (data_wrapper->data->Type() != TypeName_v<double>) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_TYPE_MISMATCH,
                std::string("Expected double, got ") + std::string(data_wrapper->data->Type()));
            return FLOW_ERROR_TYPE_MISMATCH;
        }

        try {
            auto* typed_data = static_cast<detail::NodeData<double>*>(data_wrapper->data.get());
            *value = typed_data->Get();
            return FLOW_SUCCESS;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get double value: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_data_get_bool(FlowNodeDataHandle data, bool* value) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(data, "data")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_pointer(value, "value")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!data_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        if (!data_wrapper->data) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Data is null");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        // Check if the data is of the correct type
        if (data_wrapper->data->Type() != TypeName_v<bool>) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_TYPE_MISMATCH,
                std::string("Expected bool, got ") + std::string(data_wrapper->data->Type()));
            return FLOW_ERROR_TYPE_MISMATCH;
        }

        try {
            auto* typed_data = static_cast<detail::NodeData<bool>*>(data_wrapper->data.get());
            *value = typed_data->Get();
            return FLOW_SUCCESS;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get bool value: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_data_get_string(FlowNodeDataHandle data, char** value) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(data, "data")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_pointer(value, "value")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!data_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        if (!data_wrapper->data) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Data is null");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        // Check if the data is of the correct type
        if (data_wrapper->data->Type() != TypeName_v<std::string>) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_TYPE_MISMATCH,
                std::string("Expected string, got ") + std::string(data_wrapper->data->Type()));
            return FLOW_ERROR_TYPE_MISMATCH;
        }

        try {
            auto* typed_data =
                static_cast<detail::NodeData<std::string>*>(data_wrapper->data.get());
            const std::string& str_value = typed_data->Get();

            // Allocate string that will be freed by flow_free_string
            *value = new char[str_value.length() + 1];
            std::strcpy(*value, str_value.c_str());
            return FLOW_SUCCESS;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get string value: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT const char* flow_data_get_type(FlowNodeDataHandle data) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(data, "data")) {
            return nullptr;
        }

        auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!data_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return nullptr;
        }

        if (!data_wrapper->data) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Data is null");
            return nullptr;
        }

        try {
            std::string_view type_name = data_wrapper->data->Type();

            // Allocate string that will be freed by flow_free_string
            // Convert string_view to string to ensure null-termination
            std::string type_str(type_name);
            char* result = new char[type_str.length() + 1];
            std::strcpy(result, type_str.c_str());
            return result;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get data type: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT void flow_data_destroy(FlowNodeDataHandle data) {
    FLOW_API_CALL_VOID({
        if (!flow_ffi::validate_handle(data, "data")) {
            return;
        }

        // Reference counting will handle cleanup automatically
        flow_ffi::release_handle(data);
    });
}

FLOW_FFI_EXPORT const char* flow_data_to_string(FlowNodeDataHandle data) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(data, "data")) {
            return nullptr;
        }

        auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!data_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return nullptr;
        }

        if (!data_wrapper->data) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Data is null");
            return nullptr;
        }

        try {
            std::string str_value = data_wrapper->data->ToString();

            // Allocate string that will be freed by flow_free_string
            char* result = new char[str_value.length() + 1];
            std::strcpy(result, str_value.c_str());
            return result;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to convert data to string: ") + e.what());
            return nullptr;
        }
    });
}

} // extern "C"