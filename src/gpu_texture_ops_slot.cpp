// gpu_texture_ops_slot.cpp — P12
//
// Atomic vtable slot for the non-CUDA GPU texture ops callbacks (FlowGpuTextureOps).
// Mirror of texture_ops_slot.cpp for the CUDA FlowTextureOps slot.
//
// Thread safety: written exactly once during plugin registration on the platform
// thread before any FlowFlutterTextureSink node could call it.  The atomic flag
// provides a fast-path bound check; the mutex ensures a consistent view of the
// full struct.

#include "flow_ffi.h"

#include <atomic>
#include <mutex>

static std::mutex        g_gpu_ops_mutex;
static FlowGpuTextureOps g_gpu_texture_ops{nullptr, nullptr, nullptr, nullptr, nullptr};
static std::atomic<int>  g_gpu_ops_bound{0};

extern "C" {

FLOW_FFI_EXPORT void flow_ffi_set_gpu_texture_ops(const FlowGpuTextureOps* ops) {
    std::lock_guard<std::mutex> lk(g_gpu_ops_mutex);
    if (ops &&
        ops->create_texture      != nullptr &&
        ops->destroy_texture     != nullptr &&
        ops->mark_frame_available != nullptr &&
        ops->wait_initialized    != nullptr &&
        ops->submit_frame        != nullptr)
    {
        g_gpu_texture_ops = *ops;
        g_gpu_ops_bound.store(1, std::memory_order_release);
    } else {
        g_gpu_texture_ops = FlowGpuTextureOps{nullptr, nullptr, nullptr, nullptr, nullptr};
        g_gpu_ops_bound.store(0, std::memory_order_release);
    }
}

FLOW_FFI_EXPORT int flow_ffi_is_gpu_texture_ops_bound(void) {
    return g_gpu_ops_bound.load(std::memory_order_acquire);
}

FLOW_FFI_EXPORT FlowGpuTextureOps flow_ffi_get_gpu_texture_ops(void) {
    std::lock_guard<std::mutex> lk(g_gpu_ops_mutex);
    return g_gpu_texture_ops;
}

FLOW_FFI_EXPORT bool flow_ffi_gpu_texture_sink_available(void) {
    return g_gpu_ops_bound.load(std::memory_order_acquire) != 0;
}

} // extern "C"
