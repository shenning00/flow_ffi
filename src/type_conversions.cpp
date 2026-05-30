#include "flow_ffi.h"

#include <flow/core/NodeData.hpp>

#include <cstring>

#include "error_handling.hpp"
#include "handle_manager.hpp"

using namespace flow;

// NodeDataWrapper is defined in a shared header for ODR safety across TUs —
// see node_data_wrapper.hpp for the rationale (mirrors node_wrapper.hpp).
#include "node_data_wrapper.hpp"

// Helper function to create typed NodeData.
//
// Constructs the LEAF `flow::NodeData<T>`, not the base
// `flow::detail::NodeData<T>`. Node authors typically read input values via
// the typed shorthand `node.GetInputData<T>(key)`, which does a
// dynamic_pointer_cast to `flow::NodeData<T>`. Producing the base type here
// makes that cast return null and any `Compute()` that uses the shorthand
// silently early-returns, killing the cascade.
//
// See flutter_fl_nodes/FFI_DATA_TYPE_MISMATCH.md for the diagnostic story
// and the audit of read-side static_casts (they remain correct because the
// leaf IS-A the base).
template<typename T>
SharedNodeData CreateTypedData(const T& value)
{
    return std::make_shared<NodeData<T>>(value);
}

extern "C"
{

    // ============================================================================
    // Data Creation Functions
    // ============================================================================

    FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_int(int32_t value)
    {
        FLOW_API_CALL_HANDLE({
            auto data    = CreateTypedData<int>(static_cast<int>(value));
            auto wrapper = NodeDataWrapper(data);
            return reinterpret_cast<FlowNodeDataHandle>(flow_ffi::create_handle<NodeDataWrapper>(wrapper));
        });
    }

    FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_double(double value)
    {
        FLOW_API_CALL_HANDLE({
            auto data    = CreateTypedData<double>(value);
            auto wrapper = NodeDataWrapper(data);
            return reinterpret_cast<FlowNodeDataHandle>(flow_ffi::create_handle<NodeDataWrapper>(wrapper));
        });
    }

    FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_bool(bool value)
    {
        FLOW_API_CALL_HANDLE({
            auto data    = CreateTypedData<bool>(value);
            auto wrapper = NodeDataWrapper(data);
            return reinterpret_cast<FlowNodeDataHandle>(flow_ffi::create_handle<NodeDataWrapper>(wrapper));
        });
    }

    FLOW_FFI_EXPORT FlowNodeDataHandle flow_data_create_string(const char* value)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_string(value, "value"))
            {
                return nullptr;
            }

            auto data    = CreateTypedData<std::string>(std::string(value));
            auto wrapper = NodeDataWrapper(data);
            return reinterpret_cast<FlowNodeDataHandle>(flow_ffi::create_handle<NodeDataWrapper>(wrapper));
        });
    }

    // ============================================================================
    // Data Access Functions
    // ============================================================================

    FLOW_FFI_EXPORT FlowError flow_data_get_int(FlowNodeDataHandle data, int32_t* value)
    {
        FLOW_API_CALL({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return FLOW_ERROR_INVALID_HANDLE;
            }
            if (!flow_ffi::validate_pointer(value, "value"))
            {
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
            if (!data_wrapper)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE, "Invalid data handle");
                return FLOW_ERROR_INVALID_HANDLE;
            }

            if (!data_wrapper->data)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Data is null");
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            // Check if the data is of the correct type
            if (data_wrapper->data->Type() != TypeName_v<int>)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_TYPE_MISMATCH,
                                                             std::string("Expected int, got ") +
                                                                 std::string(data_wrapper->data->Type()));
                return FLOW_ERROR_TYPE_MISMATCH;
            }

            try
            {
                auto* typed_data = static_cast<detail::NodeData<int>*>(data_wrapper->data.get());
                *value           = static_cast<int32_t>(typed_data->Get());
                return FLOW_SUCCESS;
            }
            catch (const std::exception& e)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN,
                                                             std::string("Failed to get int value: ") + e.what());
                return FLOW_ERROR_UNKNOWN;
            }
        });
    }

    // 64-bit integer access — accepts NodeData<int64_t>, NodeData<long>,
    // NodeData<long long>.  Separate symbol from flow_data_get_int because the
    // Dart-side int is 64-bit, and ports like FlowFlutterCudaPreview's "texture_id"
    // emit values that don't fit in int32_t.
    FLOW_FFI_EXPORT FlowError flow_data_get_int64(FlowNodeDataHandle data, int64_t* value)
    {
        FLOW_API_CALL({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return FLOW_ERROR_INVALID_HANDLE;
            }
            if (!flow_ffi::validate_pointer(value, "value"))
            {
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
            if (!data_wrapper)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE, "Invalid data handle");
                return FLOW_ERROR_INVALID_HANDLE;
            }

            if (!data_wrapper->data)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Data is null");
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            // Accept any of the 64-bit integer flavours flow-core might store.
            // TypeName_v<int64_t> resolves to "long" on gcc/clang Linux x86_64;
            // we list the synonyms explicitly so the check is exhaustive.
            const std::string_view t = data_wrapper->data->Type();
            if (t == TypeName_v<int64_t> || t == TypeName_v<long> || t == TypeName_v<long long> ||
                t == TypeName_v<uint64_t> || t == TypeName_v<unsigned long> || t == TypeName_v<unsigned long long>)
            {

                try
                {
                    if (t == TypeName_v<int64_t>)
                    {
                        *value = static_cast<detail::NodeData<int64_t>*>(data_wrapper->data.get())->Get();
                    }
                    else if (t == TypeName_v<long>)
                    {
                        *value =
                            static_cast<int64_t>(static_cast<detail::NodeData<long>*>(data_wrapper->data.get())->Get());
                    }
                    else if (t == TypeName_v<long long>)
                    {
                        *value = static_cast<int64_t>(
                            static_cast<detail::NodeData<long long>*>(data_wrapper->data.get())->Get());
                    }
                    else if (t == TypeName_v<uint64_t>)
                    {
                        *value = static_cast<int64_t>(
                            static_cast<detail::NodeData<uint64_t>*>(data_wrapper->data.get())->Get());
                    }
                    else if (t == TypeName_v<unsigned long>)
                    {
                        *value = static_cast<int64_t>(
                            static_cast<detail::NodeData<unsigned long>*>(data_wrapper->data.get())->Get());
                    }
                    else
                    {
                        *value = static_cast<int64_t>(
                            static_cast<detail::NodeData<unsigned long long>*>(data_wrapper->data.get())->Get());
                    }
                    return FLOW_SUCCESS;
                }
                catch (const std::exception& e)
                {
                    flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN,
                                                                 std::string("Failed to get int64 value: ") + e.what());
                    return FLOW_ERROR_UNKNOWN;
                }
            }

            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_TYPE_MISMATCH,
                                                         std::string("Expected 64-bit integer, got ") + std::string(t));
            return FLOW_ERROR_TYPE_MISMATCH;
        });
    }

    FLOW_FFI_EXPORT FlowError flow_data_get_double(FlowNodeDataHandle data, double* value)
    {
        FLOW_API_CALL({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return FLOW_ERROR_INVALID_HANDLE;
            }
            if (!flow_ffi::validate_pointer(value, "value"))
            {
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
            if (!data_wrapper)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE, "Invalid data handle");
                return FLOW_ERROR_INVALID_HANDLE;
            }

            if (!data_wrapper->data)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Data is null");
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            // Check if the data is of the correct type
            if (data_wrapper->data->Type() != TypeName_v<double>)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_TYPE_MISMATCH,
                                                             std::string("Expected double, got ") +
                                                                 std::string(data_wrapper->data->Type()));
                return FLOW_ERROR_TYPE_MISMATCH;
            }

            try
            {
                auto* typed_data = static_cast<detail::NodeData<double>*>(data_wrapper->data.get());
                *value           = typed_data->Get();
                return FLOW_SUCCESS;
            }
            catch (const std::exception& e)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN,
                                                             std::string("Failed to get double value: ") + e.what());
                return FLOW_ERROR_UNKNOWN;
            }
        });
    }

    FLOW_FFI_EXPORT FlowError flow_data_get_bool(FlowNodeDataHandle data, bool* value)
    {
        FLOW_API_CALL({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return FLOW_ERROR_INVALID_HANDLE;
            }
            if (!flow_ffi::validate_pointer(value, "value"))
            {
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
            if (!data_wrapper)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE, "Invalid data handle");
                return FLOW_ERROR_INVALID_HANDLE;
            }

            if (!data_wrapper->data)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Data is null");
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            // Check if the data is of the correct type
            if (data_wrapper->data->Type() != TypeName_v<bool>)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_TYPE_MISMATCH,
                                                             std::string("Expected bool, got ") +
                                                                 std::string(data_wrapper->data->Type()));
                return FLOW_ERROR_TYPE_MISMATCH;
            }

            try
            {
                auto* typed_data = static_cast<detail::NodeData<bool>*>(data_wrapper->data.get());
                *value           = typed_data->Get();
                return FLOW_SUCCESS;
            }
            catch (const std::exception& e)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN,
                                                             std::string("Failed to get bool value: ") + e.what());
                return FLOW_ERROR_UNKNOWN;
            }
        });
    }

    FLOW_FFI_EXPORT FlowError flow_data_get_string(FlowNodeDataHandle data, char** value)
    {
        FLOW_API_CALL({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return FLOW_ERROR_INVALID_HANDLE;
            }
            if (!flow_ffi::validate_pointer(value, "value"))
            {
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
            if (!data_wrapper)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE, "Invalid data handle");
                return FLOW_ERROR_INVALID_HANDLE;
            }

            if (!data_wrapper->data)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Data is null");
                return FLOW_ERROR_INVALID_ARGUMENT;
            }

            // Check if the data is of the correct type
            if (data_wrapper->data->Type() != TypeName_v<std::string>)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_TYPE_MISMATCH,
                                                             std::string("Expected string, got ") +
                                                                 std::string(data_wrapper->data->Type()));
                return FLOW_ERROR_TYPE_MISMATCH;
            }

            try
            {
                auto* typed_data             = static_cast<detail::NodeData<std::string>*>(data_wrapper->data.get());
                const std::string& str_value = typed_data->Get();

                // Allocate string that will be freed by flow_free_string
                *value = new char[str_value.length() + 1];
                std::strcpy(*value, str_value.c_str());
                return FLOW_SUCCESS;
            }
            catch (const std::exception& e)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN,
                                                             std::string("Failed to get string value: ") + e.what());
                return FLOW_ERROR_UNKNOWN;
            }
        });
    }

    FLOW_FFI_EXPORT const char* flow_data_get_type(FlowNodeDataHandle data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return nullptr;
            }

            auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
            if (!data_wrapper)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE, "Invalid data handle");
                return nullptr;
            }

            if (!data_wrapper->data)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Data is null");
                return nullptr;
            }

            try
            {
                std::string_view type_name = data_wrapper->data->Type();

                // Allocate string that will be freed by flow_free_string
                // Convert string_view to string to ensure null-termination
                std::string type_str(type_name);
                char* result = new char[type_str.length() + 1];
                std::strcpy(result, type_str.c_str());
                return result;
            }
            catch (const std::exception& e)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN,
                                                             std::string("Failed to get data type: ") + e.what());
                return nullptr;
            }
        });
    }

    FLOW_FFI_EXPORT void flow_data_destroy(FlowNodeDataHandle data)
    {
        FLOW_API_CALL_VOID({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return;
            }

            // Reference counting will handle cleanup automatically
            flow_ffi::release_handle(data);
        });
    }

    FLOW_FFI_EXPORT const char* flow_data_to_string(FlowNodeDataHandle data)
    {
        FLOW_API_CALL_HANDLE({
            if (!flow_ffi::validate_handle(data, "data"))
            {
                return nullptr;
            }

            auto* data_wrapper = flow_ffi::get_handle<NodeDataWrapper>(data);
            if (!data_wrapper)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE, "Invalid data handle");
                return nullptr;
            }

            if (!data_wrapper->data)
            {
                flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT, "Data is null");
                return nullptr;
            }

            try
            {
                std::string str_value = data_wrapper->data->ToString();

                // Allocate string that will be freed by flow_free_string
                char* result = new char[str_value.length() + 1];
                std::strcpy(result, str_value.c_str());
                return result;
            }
            catch (const std::exception& e)
            {
                flow_ffi::ErrorManager::instance().set_error(
                    FLOW_ERROR_UNKNOWN, std::string("Failed to convert data to string: ") + e.what());
                return nullptr;
            }
        });
    }

} // extern "C"