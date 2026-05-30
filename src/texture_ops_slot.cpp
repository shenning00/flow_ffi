// texture_ops_slot.cpp — Phase D
//
// Stores the four Flutter texture operation callbacks injected by the Linux
// plugin (packages/flow_ffi_flutter/linux/cuda_texture_bridge.cc).
//
// Rationale for a separate slot vs. baking into texture_registrar_slot.cpp:
//   texture_registrar_slot.cpp stores a single void* with no platform SDK
//   dependency and is compiled in both CUDA and non-CUDA builds.  The ops
//   structure carries typed function pointers whose semantics are specific to
//   the Phase D CUDA path.  Keeping them separate avoids polluting the public
//   header with CUDA-gated ifdefs and keeps each TU's responsibility minimal.
//
// Thread safety: the slot is written exactly once (during plugin registration
// on the platform thread) before any CUDA node could call it.  The reads in
// image_bridge_cuda.cpp happen after that write in program order.  The mutex
// ensures a consistent view of the full struct; the atomic flag provides a
// fast-path bound check without locking.

#include "flow_ffi.h"

#include <atomic>
#include <cstring>
#include <mutex>

static std::mutex g_ops_mutex;
static FlowTextureOps g_texture_ops{nullptr, nullptr, nullptr, nullptr, nullptr};
static std::atomic<int> g_ops_bound{0};

extern "C"
{

    FLOW_FFI_EXPORT void flow_ffi_set_texture_ops(const FlowTextureOps* ops)
    {
        std::lock_guard<std::mutex> lk(g_ops_mutex);
        if (ops && ops->create_gl_texture != nullptr && ops->destroy_gl_texture != nullptr &&
            ops->mark_frame_available != nullptr && ops->wait_initialized != nullptr && ops->submit_frame != nullptr)
        {
            g_texture_ops = *ops;
            g_ops_bound.store(1, std::memory_order_release);
        }
        else
        {
            g_texture_ops = FlowTextureOps{nullptr, nullptr, nullptr, nullptr, nullptr};
            g_ops_bound.store(0, std::memory_order_release);
        }
    }

    FLOW_FFI_EXPORT int flow_ffi_is_texture_ops_bound(void) { return g_ops_bound.load(std::memory_order_acquire); }

    FLOW_FFI_EXPORT FlowTextureOps flow_ffi_get_texture_ops(void)
    {
        std::lock_guard<std::mutex> lk(g_ops_mutex);
        return g_texture_ops;
    }

} // extern "C"
