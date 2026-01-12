#include "error_handling.hpp"

extern "C" {

FLOW_FFI_EXPORT const char* flow_get_last_error(void) {
    return flow_ffi::ErrorManager::instance().get_last_error();
}

FLOW_FFI_EXPORT void flow_clear_error(void) {
    flow_ffi::ErrorManager::instance().clear_error();
}

FLOW_FFI_EXPORT void flow_set_error(FlowError code, const char* message) {
    if (message) {
        flow_ffi::ErrorManager::instance().set_error(code, message);
    } else {
        flow_ffi::ErrorManager::instance().set_error(code, "Unknown error");
    }
}

} // extern "C"