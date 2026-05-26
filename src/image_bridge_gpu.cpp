// image_bridge_gpu.cpp — P12: CPU-host→Flutter texture interop
//
// Non-CUDA parallel to image_bridge_cuda.cpp.  No CUDA dependency; compiles
// cleanly in any build configuration.
//
// Architecture (mirrors image_bridge_cuda.cpp):
// ---------------------------------------------
//  register: plugin creates a FlTextureGL subclass, registers with
//    FlTextureRegistrar, arms the bootstrap re-mark loop, writes
//    (texture_object, texture_id).  Returns a FlowGpuTextureHandle (opaque
//    registry key equal to the GpuTextureState*).
//
//  upload_to_texture (hot path, per-frame, called from cascade worker):
//    1. wait_initialized: block until populate() has done glGenTextures +
//       glTexImage2D on the raster thread.
//    2. submit_frame: memcpy host bytes into the staging buffer (no GL context
//       required).  mark_frame_available is called internally by submit_frame.
//
//  unregister: remove state from registry, call destroy_texture.
//
// Synchronization:
//   Per-handle registry protected by a single mutex.  The hot path
//   (upload_to_texture) holds the mutex briefly for the look-up, then releases
//   it before the blocking wait_initialized call.

#include "flow_ffi.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace {

struct GpuTextureState {
    int64_t flutter_texture_id{-1};
    int32_t width{0};
    int32_t height{0};
    void*   flutter_texture_object{nullptr}; // GObject*, ref owned by us
};

static std::mutex g_registry_mutex;
static std::unordered_map<FlowGpuTextureHandle,
                          std::unique_ptr<GpuTextureState>> g_registry;

static GpuTextureState* find_state_locked(FlowGpuTextureHandle handle) {
    auto it = g_registry.find(handle);
    if (it == g_registry.end()) return nullptr;
    return it->second.get();
}

} // namespace

// ---------------------------------------------------------------------------
// Public FFI implementations
// ---------------------------------------------------------------------------

extern "C" FlowError flow_ffi_register_flutter_texture(
    int32_t               w,
    int32_t               h,
    FlowGpuTextureHandle* out,
    int64_t*              out_texture_id)
{
    if (out)           *out           = nullptr;
    if (out_texture_id) *out_texture_id = -1;

    if (w <= 0 || h <= 0) {
        flow_set_error(FLOW_ERROR_INVALID_ARGUMENT,
                       "flow_ffi_register_flutter_texture: "
                       "width and height must be positive");
        return FLOW_ERROR_INVALID_ARGUMENT;
    }
    if (!out || !out_texture_id) {
        flow_set_error(FLOW_ERROR_INVALID_ARGUMENT,
                       "flow_ffi_register_flutter_texture: "
                       "out and out_texture_id must not be null");
        return FLOW_ERROR_INVALID_ARGUMENT;
    }

    if (!flow_ffi_is_texture_registrar_bound()) {
        flow_set_error(FLOW_ERROR_NOT_IMPLEMENTED,
                       "flow_ffi_register_flutter_texture: "
                       "FlTextureRegistrar not yet bound — "
                       "the Flutter plugin has not registered");
        return FLOW_ERROR_NOT_IMPLEMENTED;
    }

    if (!flow_ffi_is_gpu_texture_ops_bound()) {
        flow_set_error(FLOW_ERROR_NOT_IMPLEMENTED,
                       "flow_ffi_register_flutter_texture: "
                       "GPU texture ops callbacks not yet bound — "
                       "texture_sink_bridge.cc has not injected them");
        return FLOW_ERROR_NOT_IMPLEMENTED;
    }

    FlowGpuTextureOps ops   = flow_ffi_get_gpu_texture_ops();
    void*   registrar       = flow_ffi_get_texture_registrar();

    void*   texture_object  = nullptr;
    int64_t texture_id      = -1;

    int rc = ops.create_texture(registrar, w, h, &texture_object, &texture_id);
    if (rc != 0 || !texture_object || texture_id < 0) {
        flow_set_error(FLOW_ERROR_UNKNOWN,
                       "flow_ffi_register_flutter_texture: "
                       "create_texture callback failed");
        return FLOW_ERROR_UNKNOWN;
    }

    auto state                         = std::make_unique<GpuTextureState>();
    state->flutter_texture_id          = texture_id;
    state->width                       = w;
    state->height                      = h;
    state->flutter_texture_object      = texture_object;

    FlowGpuTextureHandle handle = static_cast<FlowGpuTextureHandle>(state.get());

    {
        std::lock_guard<std::mutex> lk(g_registry_mutex);
        g_registry.emplace(handle, std::move(state));
    }

    *out            = handle;
    *out_texture_id = texture_id;
    return FLOW_SUCCESS;
}

// ---------------------------------------------------------------------------

extern "C" FlowError flow_ffi_unregister_flutter_texture(
    FlowGpuTextureHandle handle)
{
    if (!handle) {
        flow_set_error(FLOW_ERROR_INVALID_HANDLE,
                       "flow_ffi_unregister_flutter_texture: null handle");
        return FLOW_ERROR_INVALID_HANDLE;
    }

    std::unique_ptr<GpuTextureState> state;
    {
        std::lock_guard<std::mutex> lk(g_registry_mutex);
        auto it = g_registry.find(handle);
        if (it == g_registry.end()) {
            flow_set_error(FLOW_ERROR_INVALID_HANDLE,
                           "flow_ffi_unregister_flutter_texture: "
                           "handle not found in registry");
            return FLOW_ERROR_INVALID_HANDLE;
        }
        state = std::move(it->second);
        g_registry.erase(it);
    }

    if (state->flutter_texture_object) {
        if (flow_ffi_is_gpu_texture_ops_bound()) {
            FlowGpuTextureOps ops = flow_ffi_get_gpu_texture_ops();
            void* registrar      = flow_ffi_get_texture_registrar();
            ops.destroy_texture(registrar,
                                state->flutter_texture_object,
                                state->flutter_texture_id);
        }
        // If ops have been torn down (plugin unloaded), skip — Flutter engine
        // cleans up on exit.
    }

    return FLOW_SUCCESS;
}

// ---------------------------------------------------------------------------

extern "C" FlowError flow_ffi_upload_to_texture(
    FlowGpuTextureHandle handle,
    const void*          host_bytes,
    int32_t              row_stride_bytes,
    int32_t              width,
    int32_t              height)
{
    if (!handle) return FLOW_ERROR_INVALID_HANDLE;
    if (!host_bytes || row_stride_bytes <= 0 || width <= 0 || height <= 0) {
        return FLOW_ERROR_INVALID_ARGUMENT;
    }

    // Look up state; release mutex before blocking wait.
    void* tex_obj = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_registry_mutex);
        GpuTextureState* s = find_state_locked(handle);
        if (!s) {
            flow_set_error(FLOW_ERROR_INVALID_HANDLE,
                           "flow_ffi_upload_to_texture: handle not in registry");
            return FLOW_ERROR_INVALID_HANDLE;
        }
        if (width != s->width || height != s->height) {
            flow_set_error(FLOW_ERROR_INVALID_ARGUMENT,
                           "flow_ffi_upload_to_texture: dimensions mismatch — "
                           "unregister and re-register on resize");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        tex_obj = s->flutter_texture_object;
    }

    if (!flow_ffi_is_gpu_texture_ops_bound()) {
        return FLOW_SUCCESS; // ops torn down
    }
    FlowGpuTextureOps ops = flow_ffi_get_gpu_texture_ops();

    // Wait for raster thread to complete GL init (one-time cost, ~1 vsync).
    int wait_rc = ops.wait_initialized(tex_obj);
    if (wait_rc != 0) {
        // Init failed or timed out — drop frame silently.
        return FLOW_SUCCESS;
    }

    // Delegate copy to submit_frame; mark_frame_available is called internally.
    int submit_rc = ops.submit_frame(
        tex_obj,
        host_bytes,
        row_stride_bytes,
        width, height);

    // Frame drop is non-fatal — keep the cascade moving.
    (void)submit_rc;

    return FLOW_SUCCESS;
}
