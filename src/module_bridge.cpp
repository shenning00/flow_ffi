// Module bridge implementation - Phase 6
// Complete implementation of FFI bridge for flow-core Module class

#include "flow_ffi.h"

#include <flow/core/Module.hpp>
#include <flow/core/NodeFactory.hpp>

#include <cstring>
#include <filesystem>
#include <memory>

#include "env_wrapper.hpp"
#include "error_handling.hpp"
#include "handle_manager.hpp"

using namespace flow;
namespace fs = std::filesystem;

extern "C" {

// ============================================================================
// Module Management
// ============================================================================

FLOW_FFI_EXPORT FlowModuleHandle flow_module_create(FlowNodeFactoryHandle factory) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        // Factory is stored as NodeFactoryWrapper, not std::shared_ptr<NodeFactory>
        auto* factory_wrapper = flow_ffi::get_handle<NodeFactoryWrapper>(factory);
        if (!factory_wrapper || !factory_wrapper->factory) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid factory handle");
            return nullptr;
        }

        auto module = std::make_shared<Module>(factory_wrapper->factory);
        return static_cast<FlowModuleHandle>(
            flow_ffi::create_handle<std::shared_ptr<Module>>(std::move(module)));

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_OUT_OF_MEMORY, e.what());
        return nullptr;
    }
}

FLOW_FFI_EXPORT void flow_module_destroy(FlowModuleHandle module) {
    if (module && flow_ffi::is_valid_handle(module)) {
        flow_ffi::release_handle(module);
    }
}

FLOW_FFI_EXPORT FlowError flow_module_load(FlowModuleHandle module, const char* path) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module || !path) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle or path");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        // Check for empty path
        if (strlen(path) == 0) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Path cannot be empty");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        fs::path module_path(path);
        if (!fs::exists(module_path)) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "Module path does not exist");
            return FLOW_ERROR_MODULE_LOAD_FAILED;
        }

        bool success = (*module_ptr)->Load(module_path);
        if (!success) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "Failed to load module");
            return FLOW_ERROR_MODULE_LOAD_FAILED;
        }

        return FLOW_SUCCESS;

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED, e.what());
        return FLOW_ERROR_MODULE_LOAD_FAILED;
    }
}

FLOW_FFI_EXPORT FlowError flow_module_unload(FlowModuleHandle module) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        // Check if module is loaded first
        if (!(*module_ptr)->IsLoaded()) {
            // Unloading a module that's not loaded is a no-op and should succeed
            return FLOW_SUCCESS;
        }

        bool success = (*module_ptr)->Unload();
        if (!success) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "Failed to unload module");
            return FLOW_ERROR_MODULE_LOAD_FAILED;
        }

        return FLOW_SUCCESS;

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED, e.what());
        return FLOW_ERROR_MODULE_LOAD_FAILED;
    }
}

FLOW_FFI_EXPORT FlowError flow_module_register_nodes(FlowModuleHandle module) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        if (!(*module_ptr)->IsLoaded()) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "Module is not loaded");
            return FLOW_ERROR_MODULE_LOAD_FAILED;
        }

        (*module_ptr)->RegisterModuleNodes();
        return FLOW_SUCCESS;

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED, e.what());
        return FLOW_ERROR_MODULE_LOAD_FAILED;
    }
}

FLOW_FFI_EXPORT FlowError flow_module_unregister_nodes(FlowModuleHandle module) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        if (!(*module_ptr)->IsLoaded()) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "Module is not loaded");
            return FLOW_ERROR_MODULE_LOAD_FAILED;
        }

        (*module_ptr)->UnregisterModuleNodes();
        return FLOW_SUCCESS;

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED, e.what());
        return FLOW_ERROR_MODULE_LOAD_FAILED;
    }
}

FLOW_FFI_EXPORT bool flow_module_is_loaded(FlowModuleHandle module) {
    try {
        if (!module) {
            return false;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            return false;
        }

        return (*module_ptr)->IsLoaded();

    } catch (const std::exception&) {
        return false;
    }
}

FLOW_FFI_EXPORT const char* flow_module_get_name(FlowModuleHandle module) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle");
            return nullptr;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return nullptr;
        }

        const auto& metadata = (*module_ptr)->GetMetaData();
        if (!metadata) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "No metadata available");
            return nullptr;
        }

        // Return pointer to internal string - valid as long as module exists
        return metadata->Name.c_str();

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN, e.what());
        return nullptr;
    }
}

FLOW_FFI_EXPORT const char* flow_module_get_version(FlowModuleHandle module) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle");
            return nullptr;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return nullptr;
        }

        const auto& metadata = (*module_ptr)->GetMetaData();
        if (!metadata) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "No metadata available");
            return nullptr;
        }

        return metadata->Version.c_str();

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN, e.what());
        return nullptr;
    }
}

FLOW_FFI_EXPORT const char* flow_module_get_author(FlowModuleHandle module) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle");
            return nullptr;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return nullptr;
        }

        const auto& metadata = (*module_ptr)->GetMetaData();
        if (!metadata) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "No metadata available");
            return nullptr;
        }

        return metadata->Author.c_str();

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN, e.what());
        return nullptr;
    }
}

FLOW_FFI_EXPORT const char* flow_module_get_description(FlowModuleHandle module) {
    try {
        flow_ffi::ErrorManager::instance().clear_error();

        if (!module) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                                         "Invalid module handle");
            return nullptr;
        }

        auto* module_ptr = flow_ffi::get_handle<std::shared_ptr<Module>>(module);
        if (!module_ptr) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid module handle");
            return nullptr;
        }

        const auto& metadata = (*module_ptr)->GetMetaData();
        if (!metadata) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_MODULE_LOAD_FAILED,
                                                         "No metadata available");
            return nullptr;
        }

        return metadata->Description.c_str();

    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_UNKNOWN, e.what());
        return nullptr;
    }
}

} // extern "C"