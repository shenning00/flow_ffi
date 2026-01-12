#include "handle_manager.hpp"

#include "flow_ffi.h"

extern "C" {

FLOW_FFI_EXPORT bool flow_is_valid_handle(void* handle) {
    return flow_ffi::is_valid_handle(handle);
}

FLOW_FFI_EXPORT void flow_retain_handle(void* handle) {
    flow_ffi::retain_handle(handle);
}

FLOW_FFI_EXPORT void flow_release_handle(void* handle) {
    flow_ffi::release_handle(handle);
}

FLOW_FFI_EXPORT int32_t flow_get_ref_count(void* handle) {
    return flow_ffi::get_ref_count(handle);
}

} // extern "C"