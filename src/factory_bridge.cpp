#include "flow_ffi.h"

#include <flow/core/Env.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/NodeFactory.hpp>

#include <cstring>

#include "env_wrapper.hpp"
#include "error_handling.hpp"
#include "handle_manager.hpp"

using namespace flow;

// Wrapper structure for Node
struct NodeWrapper {
    SharedNode node;

    NodeWrapper(SharedNode n) : node(std::move(n)) {}
};

extern "C" {

FLOW_FFI_EXPORT FlowNodeHandle flow_factory_create_node(FlowNodeFactoryHandle factory,
                                                        const char* class_name, const char* uuid,
                                                        const char* name, FlowEnvHandle env) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(factory, "factory")) {
            return nullptr;
        }
        if (!flow_ffi::validate_handle(env, "env")) {
            return nullptr;
        }
        if (!flow_ffi::validate_string(class_name, "class_name")) {
            return nullptr;
        }

        auto* factory_wrapper = flow_ffi::get_handle<NodeFactoryWrapper>(factory);
        if (!factory_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid factory handle");
            return nullptr;
        }

        auto* env_wrapper = flow_ffi::get_handle<EnvWrapper>(env);
        if (!env_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid environment handle");
            return nullptr;
        }

        try {
            UUID node_uuid;
            if (uuid && strlen(uuid) > 0) {
                node_uuid = UUID(uuid);
            } else {
                node_uuid = UUID(); // Generate new UUID
            }

            std::string node_name = name ? name : "";

            auto node = factory_wrapper->factory->CreateNode(class_name, node_uuid, node_name,
                                                             env_wrapper->env);

            if (!node) {
                flow_ffi::ErrorManager::instance().set_error(
                    FLOW_ERROR_NODE_NOT_FOUND,
                    std::string("Failed to create node of class: ") + class_name);
                return nullptr;
            }

            // Create wrapper and handle
            auto wrapper = NodeWrapper(node);
            return reinterpret_cast<FlowNodeHandle>(flow_ffi::create_handle<NodeWrapper>(wrapper));

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Node creation failed: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_factory_get_categories(FlowNodeFactoryHandle factory,
                                                      char*** categories, size_t* count) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(factory, "factory")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_pointer(categories, "categories")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        if (!flow_ffi::validate_pointer(count, "count")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* factory_wrapper = flow_ffi::get_handle<NodeFactoryWrapper>(factory);
        if (!factory_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid factory handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            const auto& category_map = factory_wrapper->factory->GetCategories();

            // Get unique category names
            std::set<std::string> unique_categories;
            for (const auto& pair : category_map) {
                unique_categories.insert(pair.first);
            }

            *count = unique_categories.size();
            if (*count == 0) {
                *categories = nullptr;
                return FLOW_SUCCESS;
            }

            // Allocate array of string pointers
            *categories = new char*[*count];

            size_t i = 0;
            for (const auto& category : unique_categories) {
                (*categories)[i] = new char[category.length() + 1];
                std::strcpy((*categories)[i], category.c_str());
                ++i;
            }

            return FLOW_SUCCESS;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get categories: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT FlowError flow_factory_get_node_classes(FlowNodeFactoryHandle factory,
                                                        const char* category, char*** classes,
                                                        size_t* count) {
    FLOW_API_CALL({
        if (!flow_ffi::validate_handle(factory, "factory")) {
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (!flow_ffi::validate_string(category, "category")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        if (!flow_ffi::validate_pointer(classes, "classes")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        if (!flow_ffi::validate_pointer(count, "count")) {
            return FLOW_ERROR_INVALID_ARGUMENT;
        }

        auto* factory_wrapper = flow_ffi::get_handle<NodeFactoryWrapper>(factory);
        if (!factory_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid factory handle");
            return FLOW_ERROR_INVALID_HANDLE;
        }

        try {
            const auto& category_map = factory_wrapper->factory->GetCategories();

            // Find all classes in the specified category
            std::vector<std::string> class_names;
            auto range = category_map.equal_range(category);
            for (auto it = range.first; it != range.second; ++it) {
                class_names.push_back(it->second);
            }

            *count = class_names.size();
            if (*count == 0) {
                *classes = nullptr;
                return FLOW_SUCCESS;
            }

            // Allocate array of string pointers
            *classes = new char*[*count];

            for (size_t i = 0; i < *count; ++i) {
                (*classes)[i] = new char[class_names[i].length() + 1];
                std::strcpy((*classes)[i], class_names[i].c_str());
            }

            return FLOW_SUCCESS;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get node classes: ") + e.what());
            return FLOW_ERROR_UNKNOWN;
        }
    });
}

FLOW_FFI_EXPORT const char* flow_factory_get_friendly_name(FlowNodeFactoryHandle factory,
                                                           const char* class_name) {
    FLOW_API_CALL_HANDLE({
        if (!flow_ffi::validate_handle(factory, "factory")) {
            return nullptr;
        }
        if (!flow_ffi::validate_string(class_name, "class_name")) {
            return nullptr;
        }

        auto* factory_wrapper = flow_ffi::get_handle<NodeFactoryWrapper>(factory);
        if (!factory_wrapper) {
            flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                         "Invalid factory handle");
            return nullptr;
        }

        try {
            std::string friendly_name = factory_wrapper->factory->GetFriendlyName(class_name);

            // Allocate string that will be freed by flow_free_string
            char* result = new char[friendly_name.length() + 1];
            std::strcpy(result, friendly_name.c_str());
            return result;

        } catch (const std::exception& e) {
            flow_ffi::ErrorManager::instance().set_error(
                FLOW_ERROR_UNKNOWN, std::string("Failed to get friendly name: ") + e.what());
            return nullptr;
        }
    });
}

FLOW_FFI_EXPORT bool flow_factory_is_convertible(FlowNodeFactoryHandle factory,
                                                 const char* from_type, const char* to_type) {
    if (!flow_ffi::validate_handle(factory, "factory")) {
        return false;
    }
    if (!flow_ffi::validate_string(from_type, "from_type")) {
        return false;
    }
    if (!flow_ffi::validate_string(to_type, "to_type")) {
        return false;
    }

    auto* factory_wrapper = flow_ffi::get_handle<NodeFactoryWrapper>(factory);
    if (!factory_wrapper) {
        flow_ffi::ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                                     "Invalid factory handle");
        return false;
    }

    try {
        return factory_wrapper->factory->IsConvertible(from_type, to_type);
    } catch (const std::exception& e) {
        flow_ffi::ErrorManager::instance().set_error(
            FLOW_ERROR_UNKNOWN, std::string("Failed to check convertibility: ") + e.what());
        return false;
    }
}

} // extern "C"