#include "flow_ffi.h"

#include <flow/core/Env.hpp>
#include <flow/core/NodeFactory.hpp>

#include <cstring>

#include "env_wrapper.hpp"
#include "error_handling.hpp"
#include "handle_manager.hpp"

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