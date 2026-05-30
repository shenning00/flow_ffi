#ifndef FLOW_FFI_CUDA_H
#define FLOW_FFI_CUDA_H

// flow_ffi_cuda.h — CUDA image-path FFI surface (P13)
//
// The three capability-discovery functions (flow_ffi_cuda_available,
// flow_ffi_cuda_device_count, flow_ffi_cuda_device_name) are ALWAYS present
// in libflow_ffi.so regardless of whether it was compiled with CUDA.  When
// compiled without CUDA they return false/0/"".  Dart calls them
// unconditionally at startup and gates GPU-preview UI on the result.
//
// The remaining texture/write/signal functions are present in both builds
// but return FLOW_ERROR_NOT_IMPLEMENTED when FLOW_FFI_HAS_CUDA == 0
// (i.e. the library was not compiled with CUDA support).
//
// cudaEvent_t in the public header
// ---------------------------------
// flow_ffi_cuda_write_into takes a `producer_ready_event` argument.  Rather
// than exposing `cudaEvent_t` (which requires cuda_runtime.h in every TU that
// includes this header), the parameter is typed as `void*`.  cudaEvent_t is
// already a typedef for void* on all supported platforms, so the ABI is
// identical.  Callers compiled with CUDA can cast transparently:
//   flow_ffi_cuda_write_into(h, ptr, pitch, w, h, (void*)my_cuda_event);
// Passing nullptr means "no synchronization" — the implementation will skip
// cudaStreamWaitEvent.

#include "flow_ffi.h" // FlowError, FLOW_FFI_EXPORT

#ifdef __cplusplus
extern "C"
{
#endif

    // Opaque handle representing a registered Flutter texture + CUDA interop state.
    typedef void* FlowCudaTextureHandle;

    // ============================================================================
    // Capability discovery — always safe to call, even when FLOW_FFI_HAS_CUDA=0
    // ============================================================================

    FLOW_FFI_EXPORT bool flow_ffi_cuda_available(void);
    FLOW_FFI_EXPORT int flow_ffi_cuda_device_count(void);
    // Returns a borrowed string valid for the lifetime of the process.
    // Returns "" for out-of-range indices or when CUDA is not available.
    FLOW_FFI_EXPORT const char* flow_ffi_cuda_device_name(int device);

    // ============================================================================
    // Texture registration
    // ============================================================================

    // Allocate a platform texture (GL/D3D11/Vulkan per platform) sized w*h RGBA8,
    // register it with Flutter's TextureRegistry, and interop-register it with
    // CUDA so flow_ffi_cuda_write_into can target it.
    // On success, *out_handle and *out_flutter_texture_id are populated.
    // Phase D will implement the bodies; currently returns FLOW_ERROR_NOT_IMPLEMENTED.
    FLOW_FFI_EXPORT FlowError flow_ffi_register_cuda_flutter_texture(int32_t width, int32_t height,
                                                                     FlowCudaTextureHandle* out_handle,
                                                                     int64_t* out_flutter_texture_id);

    FLOW_FFI_EXPORT FlowError flow_ffi_unregister_cuda_flutter_texture(FlowCudaTextureHandle handle);

    // ============================================================================
    // Per-frame write
    // ============================================================================

    // Copy from a CUDA device buffer into the registered texture, synchronizing
    // against the producer's ready event.  Called from
    // FlowFlutterCudaPreview::Compute() (Phase E).
    //
    // producer_ready_event: cast from cudaEvent_t; may be nullptr (no sync).
    // Phase D will implement the body; currently returns FLOW_ERROR_NOT_IMPLEMENTED.
    FLOW_FFI_EXPORT FlowError flow_ffi_cuda_write_into(FlowCudaTextureHandle handle, const void* src_device_ptr,
                                                       int32_t src_pitch_bytes, int32_t width, int32_t height,
                                                       void* producer_ready_event);

    // Signal Flutter that a new frame is ready on the given texture id.
    // The Texture widget will rebuild on the next pump.
    // Phase D will implement the body; currently a no-op.
    FLOW_FFI_EXPORT void flow_ffi_signal_frame_available(int64_t flutter_texture_id);

#ifdef __cplusplus
}
#endif

#endif // FLOW_FFI_CUDA_H
