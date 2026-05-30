// image_bridge_cuda_stub.cpp
// Compiled ONLY when FLOW_FFI_HAS_CUDA=0.
//
// Provides stub implementations for the 4 CUDA texture FFI symbols that
// image_bridge_cuda.cpp implements in the CUDA build.  This ensures the
// symbols are always present in libflow_ffi.so regardless of build mode,
// allowing the test binary (which links flow_ffi) to resolve them without
// #if FLOW_FFI_HAS_CUDA guards in every test file.
//
// Calling any of these in a non-CUDA build returns FLOW_ERROR_NOT_IMPLEMENTED
// (for the FlowError-returning functions) or is a no-op (signal_frame_available).

#include "flow_ffi.h"
#include "flow_ffi_cuda.h"

extern "C" FlowError flow_ffi_register_cuda_flutter_texture(int32_t /*width*/, int32_t /*height*/,
                                                            FlowCudaTextureHandle* out_handle,
                                                            int64_t* out_flutter_texture_id)
{
    if (out_handle) *out_handle = nullptr;
    if (out_flutter_texture_id) *out_flutter_texture_id = -1;
    return FLOW_ERROR_NOT_IMPLEMENTED;
}

extern "C" FlowError flow_ffi_unregister_cuda_flutter_texture(FlowCudaTextureHandle /*handle*/)
{
    return FLOW_ERROR_NOT_IMPLEMENTED;
}

extern "C" FlowError flow_ffi_cuda_write_into(FlowCudaTextureHandle /*handle*/, const void* /*src_device_ptr*/,
                                              int32_t /*src_pitch_bytes*/, int32_t /*width*/, int32_t /*height*/,
                                              void* /*producer_ready_event*/)
{
    return FLOW_ERROR_NOT_IMPLEMENTED;
}

extern "C" void flow_ffi_signal_frame_available(int64_t /*flutter_texture_id*/)
{
    // No-op in stub build.
}
