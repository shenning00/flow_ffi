#pragma once

#include "flow_ffi.h"

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace flow_ffi {

// Thread-local error management
class ErrorManager {
public:
    static ErrorManager& instance() {
        static ErrorManager manager;
        return manager;
    }

    void set_error(FlowError code, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto thread_id = std::this_thread::get_id();
        errors_[thread_id] = {code, message};
    }

    void clear_error() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto thread_id = std::this_thread::get_id();
        errors_.erase(thread_id);
    }

    const char* get_last_error() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto thread_id = std::this_thread::get_id();
        auto it = errors_.find(thread_id);
        if (it != errors_.end()) {
            return it->second.message.c_str();
        }
        return nullptr;
    }

    FlowError get_last_error_code() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto thread_id = std::this_thread::get_id();
        auto it = errors_.find(thread_id);
        if (it != errors_.end()) {
            return it->second.code;
        }
        return FLOW_SUCCESS;
    }

private:
    struct ErrorInfo {
        FlowError code;
        std::string message;
    };

    std::mutex mutex_;
    std::unordered_map<std::thread::id, ErrorInfo> errors_;
};

// RAII error setter that also handles exceptions
class ErrorSetter {
public:
    ErrorSetter() : error_set_(false) {}

    ~ErrorSetter() {
        if (!error_set_ && std::uncaught_exceptions() > 0) {
            // An exception was thrown, set a generic error
            set_error(FLOW_ERROR_UNKNOWN, "Unhandled C++ exception occurred");
        }
    }

    void set_error(FlowError code, const std::string& message) {
        ErrorManager::instance().set_error(code, message);
        error_set_ = true;
    }

    void clear_error() {
        ErrorManager::instance().clear_error();
        error_set_ = true;
    }

private:
    bool error_set_;
};

// Macro for safe API calls with exception handling
#define FLOW_API_CALL(code)                                                           \
    do {                                                                              \
        flow_ffi::ErrorSetter error_setter;                                           \
        try {                                                                         \
            code                                                                      \
        } catch (const std::exception& e) {                                           \
            error_setter.set_error(FLOW_ERROR_UNKNOWN, e.what());                     \
            return FLOW_ERROR_UNKNOWN;                                                \
        } catch (...) {                                                               \
            error_setter.set_error(FLOW_ERROR_UNKNOWN, "Unknown exception occurred"); \
            return FLOW_ERROR_UNKNOWN;                                                \
        }                                                                             \
    } while (0)

// Macro for safe API calls that return handles
#define FLOW_API_CALL_HANDLE(code)                                                    \
    do {                                                                              \
        flow_ffi::ErrorSetter error_setter;                                           \
        try {                                                                         \
            code                                                                      \
        } catch (const std::exception& e) {                                           \
            error_setter.set_error(FLOW_ERROR_UNKNOWN, e.what());                     \
            return nullptr;                                                           \
        } catch (...) {                                                               \
            error_setter.set_error(FLOW_ERROR_UNKNOWN, "Unknown exception occurred"); \
            return nullptr;                                                           \
        }                                                                             \
    } while (0)

// Macro for safe API calls that return void
#define FLOW_API_CALL_VOID(code)                                                      \
    do {                                                                              \
        flow_ffi::ErrorSetter error_setter;                                           \
        try {                                                                         \
            code                                                                      \
        } catch (const std::exception& e) {                                           \
            error_setter.set_error(FLOW_ERROR_UNKNOWN, e.what());                     \
            return;                                                                   \
        } catch (...) {                                                               \
            error_setter.set_error(FLOW_ERROR_UNKNOWN, "Unknown exception occurred"); \
            return;                                                                   \
        }                                                                             \
    } while (0)

// Validation helpers
inline bool validate_handle(void* handle, const char* handle_name) {
    if (!handle) {
        ErrorManager::instance().set_error(
            FLOW_ERROR_INVALID_HANDLE, std::string("Invalid handle: ") + handle_name + " is null");
        return false;
    }
    if (!flow_is_valid_handle(handle)) {
        ErrorManager::instance().set_error(FLOW_ERROR_INVALID_HANDLE,
                                           std::string("Invalid handle: ") + handle_name +
                                               " is not registered");
        return false;
    }
    return true;
}

inline bool validate_string(const char* str, const char* param_name) {
    if (!str) {
        ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                           std::string("Invalid argument: ") + param_name +
                                               " is null");
        return false;
    }
    return true;
}

inline bool validate_pointer(void* ptr, const char* param_name) {
    if (!ptr) {
        ErrorManager::instance().set_error(FLOW_ERROR_INVALID_ARGUMENT,
                                           std::string("Invalid argument: ") + param_name +
                                               " is null");
        return false;
    }
    return true;
}

} // namespace flow_ffi