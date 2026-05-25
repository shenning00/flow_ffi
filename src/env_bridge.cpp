#include "flow_ffi.h"

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/NodeFactory.hpp>

#include <cstring>
#include <string>

#include <flow_wire/Register.hpp>

#include "env_wrapper.hpp"
#include "error_handling.hpp"
#include "handle_manager.hpp"

// Included in preparation for P1 (flow_env_add_task_set_input_data) which
// will call get_handle<NodeDataWrapper>(data) from this TU.  Bringing the
// include in now keeps the P1 patch focused on the new symbol body.
#include "node_data_wrapper.hpp"

using namespace flow;

extern "C" {

FLOW_FFI_EXPORT FlowEnvHandle flow_env_create(int32_t max_threads) {
    FLOW_API_CALL_HANDLE({
        if (max_threads <= 0) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "max_threads must be positive");
            return nullptr;
        }

        // Create NodeFactory first
        auto factory = std::make_shared<NodeFactory>();

        // Create settings
        Settings settings;
        settings.MaxThreads = static_cast<std::size_t>(max_threads);

        // Create environment
        auto env = Env::Create(factory, settings);

        // Register the wire-type builtin nodes (ImageOpenNode, PreviewNode)
        // and any optionally-compiled adapter nodes (cv::Mat ↔ flow::Image
        // when FLOW_INTERWORK_WITH_OPENCV is set in the flow_wire build).
        // See flow_wire/Register.hpp.
        flow_wire::RegisterAllNodes(*env);

        // Create wrapper and handle
        auto wrapper = EnvWrapper(env);
        return reinterpret_cast<FlowEnvHandle>(flow_ffi::create_handle<EnvWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT void flow_env_destroy(FlowEnvHandle env) {
    FLOW_API_CALL_VOID({
        if (!flow_ffi::validate_handle(env, "env")) {
            return;
        }

        // Reference counting will handle cleanup automatically
        flow_ffi::release_handle(env);
    });
}

FLOW_FFI_EXPORT FlowNodeFactoryHandle flow_env_get_factory(FlowEnvHandle env) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(env, "env")) {
            return nullptr;
        }

        auto* env_wrapper = flow_ffi::get_handle<EnvWrapper>(env);
        if (!env_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid environment handle");
            return nullptr;
        }

        auto factory = env_wrapper->env->GetFactory();
        if (!factory) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN,
                                                         "Environment has no factory");
            return nullptr;
        }

        // Create wrapper and handle
        auto wrapper = NodeFactoryWrapper(factory);
        return reinterpret_cast<FlowNodeFactoryHandle>(
            flow_ffi::create_handle<NodeFactoryWrapper>(wrapper));
    });
}

FLOW_FFI_EXPORT FlowError flow_env_wait(FlowEnvHandle env) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(env, "env")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }

        auto* env_wrapper = flow_ffi::get_handle<EnvWrapper>(env);
        if (!env_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid environment handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        env_wrapper->env->Wait();
        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT FlowError flow_env_add_task_set_input_data(
        FlowEnvHandle env, FlowNodeHandle node,
        const char* port_key, FlowNodeDataHandle data) {
    FLOW_API_CALL({
        // Validate all three handles.
        if (!flow_ffi::validate_handle(env, "env")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_handle(node, "node")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_handle(data, "data")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }

        // Validate port_key: must be non-null and non-empty.
        if (port_key == nullptr || port_key[0] == '\0') {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_INVALID_ARGUMENT,
                "Invalid argument: port_key must not be null or empty");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* ew = flow_ffi::get_handle<EnvWrapper>(env);
        if (!ew) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid environment handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        auto* nw = flow_ffi::get_handle<NodeWrapper>(node);
        if (!nw) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid node handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        auto* dw = flow_ffi::get_handle<NodeDataWrapper>(data);
        if (!dw) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid data handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        // Capture shared_ptr copies by value into the lambda. The FFI handle
        // refcount is NOT touched — the lambda owns its own shared_ptr refs.
        // port_key is a caller C-string that may be freed after this returns,
        // so we copy it into a std::string and construct IndexableName inside
        // the lambda body (IndexableName holds a string_view, not an owned
        // string, so constructing it from port_key here would leave a dangling
        // view by the time the worker runs).
        flow::SharedNode     cn = nw->node;
        flow::SharedNodeData cd = dw->data;
        std::string          k_str(port_key);   // owned copy — survives the lambda

        // Note: Env::AddTask wraps the callable in [=]{task();} which requires
        // the lambda's operator() to be const-callable.  We therefore avoid
        // `mutable` by passing `cd` as a copy to SetInputData instead of
        // std::move — functionally equivalent; the shared_ptr lifetime
        // guarantee is preserved either way (the lambda holds its own ref).
        ew->env->AddTask([cn, k_str, cd]() {
            flow::IndexableName k(k_str);   // built from the owned std::string
            std::lock_guard<flow::Node> lock(*cn);
            cn->SetInputData(k, cd, /*compute=*/true);
        });

        return FLOW_SUCCESS;
    });
}

FLOW_FFI_EXPORT const char* flow_env_get_var(FlowEnvHandle env, const char* name) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(env, "env")) {
            return nullptr;
        }

        if (!flow_ffi::validate_string(name, "name")) {
            return nullptr;
        }

        auto* env_wrapper = flow_ffi::get_handle<EnvWrapper>(env);
        if (!env_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid environment handle");
            return nullptr;
        }

        try {
            std::string value = env_wrapper->env->GetVar(name);

            // Allocate string that will be freed by flow_free_string
            char* result = new char[value.length() + 1];
            std::strcpy(result, value.c_str());
            return result;
        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get environment variable: ") + e.what());
            return nullptr;
        }
    });
}

} // extern "C"