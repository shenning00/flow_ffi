// image_bridge_cuda.cpp — Phase D: CUDA/Flutter texture interop
// Compiled ONLY when FLOW_FFI_HAS_CUDA=1.
//
// Architecture decision (plugin-extension pattern):
// -------------------------------------------------
// libflow_ffi.so cannot include flutter_linux headers directly because those
// headers (FlTextureGL, FlTextureRegistrar, etc.) are only present inside the
// ephemeral/ tree populated by `flutter build linux`.  They are not on any
// system include path and are unavailable when flow_ffi is built standalone.
//
// Instead the Linux Flutter plugin (packages/flow_ffi_flutter/linux/
// cuda_texture_bridge.cc) implements the FlTextureGL GObject subclass and
// injects three typed function-pointer callbacks at registration time via
// flow_ffi_set_texture_ops() (declared in flow_ffi.h).  This file calls those
// callbacks to create/destroy Flutter GL textures and to signal frame-ready.
//
// CUDA/GL interop model:
// ----------------------
//  register:
//    plugin creates GL texture (name N) + FlTextureGL → returns N and texture_id
//    this file calls cudaGraphicsGLRegisterImage(cuda_resource, N, GL_TEXTURE_2D,
//        cudaGraphicsRegisterFlagsWriteDiscard)
//
//  write_into (hot path, per-frame):
//    cudaGraphicsMapResources → get CUDA array → cudaMemcpy2DToArrayAsync →
//    cudaGraphicsUnmapResources  (all on per-state stream)
//
//  signal_frame_available:
//    calls ops.mark_frame_available(registrar, texture_object)
//    Flutter compositor picks up the new frame on next vsync.
//
// Synchronization:
//   Each registered texture owns a dedicated cudaStream_t (per-state stream).
//   This avoids serialising multiple concurrent sinks on a single global stream.
//   The write_into function optionally synchronises against a caller-supplied
//   producer event (cast from cudaEvent_t via void*).
//
// Error handling:
//   RAII wrappers (CudaStreamGuard, CudaGraphicsResourceGuard) ensure cleanup
//   in error paths.  The registry map is protected by a single mutex; the hot
//   path (write_into) also acquires this mutex for the initial look-up but
//   releases it before any CUDA blocking operations to avoid holding it during
//   GPU work.

#include "flow_ffi_cuda.h"
#include "flow_ffi.h"

#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <cstdio>

// ---------------------------------------------------------------------------
// Internal helpers / RAII
// ---------------------------------------------------------------------------

namespace {

// Log helper — not exported, purely for internal diagnostics.
static void cuda_log_error(const char* context, cudaError_t err) {
    std::fprintf(stderr,
        "[flow_ffi CUDA] %s failed: %s (%d)\n",
        context, cudaGetErrorString(err), static_cast<int>(err));
}

// RAII: owns a cudaStream_t and destroys it on scope exit.
struct CudaStreamGuard {
    cudaStream_t stream{nullptr};
    bool         released{false};

    CudaStreamGuard() {
        cudaError_t e = cudaStreamCreate(&stream);
        if (e != cudaSuccess) {
            cuda_log_error("cudaStreamCreate", e);
            stream = nullptr;
        }
    }
    ~CudaStreamGuard() {
        if (!released && stream) {
            cudaStreamDestroy(stream);
        }
    }
    cudaStream_t release() {
        released = true;
        return stream;
    }
    bool ok() const { return stream != nullptr; }
};

// RAII: owns a cudaGraphicsResource_t and unregisters it on scope exit.
struct CudaGraphicsResourceGuard {
    cudaGraphicsResource_t res{nullptr};
    bool                   released{false};

    explicit CudaGraphicsResourceGuard(cudaGraphicsResource_t r) : res(r) {}
    ~CudaGraphicsResourceGuard() {
        if (!released && res) {
            cudaGraphicsUnregisterResource(res);
        }
    }
    cudaGraphicsResource_t release() {
        released = true;
        return res;
    }
};

// ---------------------------------------------------------------------------
// Per-texture state
// ---------------------------------------------------------------------------

struct CudaTextureState {
    uint32_t               gl_texture_name{0};
    // cuda_resource is written by the plugin's wait_initialized callback once
    // populate() has completed GL+CUDA registration on the raster thread.
    // It must not be accessed until wait_initialized returns success.
    cudaGraphicsResource_t cuda_resource{nullptr};
    int64_t                flutter_texture_id{-1};
    int32_t                width{0};
    int32_t                height{0};
    void*                  flutter_texture_object{nullptr}; // GObject*, ref owned by us
    cudaStream_t           stream{nullptr};
};

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

static std::mutex                                           g_registry_mutex;
static std::unordered_map<FlowCudaTextureHandle,
                          std::unique_ptr<CudaTextureState>> g_registry;

// Look up state; caller must hold g_registry_mutex.
static CudaTextureState* find_state_locked(FlowCudaTextureHandle handle) {
    auto it = g_registry.find(handle);
    if (it == g_registry.end()) return nullptr;
    return it->second.get();
}

} // namespace

// ---------------------------------------------------------------------------
// Public FFI implementations
// ---------------------------------------------------------------------------

extern "C" FlowError flow_ffi_register_cuda_flutter_texture(
    int32_t                width,
    int32_t                height,
    FlowCudaTextureHandle* out_handle,
    int64_t*               out_flutter_texture_id)
{
    // Clear outputs first.
    if (out_handle)             *out_handle             = nullptr;
    if (out_flutter_texture_id) *out_flutter_texture_id = -1;

    // (1) Validate arguments.
    if (width <= 0 || height <= 0) {
        flow_set_error(FLOW_ERROR_INVALID_ARGUMENT,
                       "flow_ffi_register_cuda_flutter_texture: "
                       "width and height must be positive");
        return FLOW_ERROR_INVALID_ARGUMENT;
    }
    if (!out_handle || !out_flutter_texture_id) {
        flow_set_error(FLOW_ERROR_INVALID_ARGUMENT,
                       "flow_ffi_register_cuda_flutter_texture: "
                       "out_handle and out_flutter_texture_id must not be null");
        return FLOW_ERROR_INVALID_ARGUMENT;
    }

    // (2) Confirm the texture registrar is bound (plugin has loaded).
    if (!flow_ffi_is_texture_registrar_bound()) {
        flow_set_error(FLOW_ERROR_NOT_IMPLEMENTED,
                       "flow_ffi_register_cuda_flutter_texture: "
                       "FlTextureRegistrar not yet bound — "
                       "the Flutter plugin has not registered");
        return FLOW_ERROR_NOT_IMPLEMENTED;
    }

    // (3) Confirm the texture ops callbacks are injected.
    if (!flow_ffi_is_texture_ops_bound()) {
        flow_set_error(FLOW_ERROR_NOT_IMPLEMENTED,
                       "flow_ffi_register_cuda_flutter_texture: "
                       "texture ops callbacks not yet bound — "
                       "cuda_texture_bridge.cc has not injected them");
        return FLOW_ERROR_NOT_IMPLEMENTED;
    }

    FlowTextureOps ops = flow_ffi_get_texture_ops();
    void*   registrar  = flow_ffi_get_texture_registrar();

    // (4) Create CUDA stream for this texture.
    CudaStreamGuard stream_guard;
    if (!stream_guard.ok()) {
        flow_set_error(FLOW_ERROR_UNKNOWN,
                       "flow_ffi_register_cuda_flutter_texture: "
                       "failed to create cudaStream");
        return FLOW_ERROR_UNKNOWN;
    }

    // (5) Ask the plugin to create a Flutter GL texture and return:
    //     - flutter_texture_object  (GObject* owned by caller of this fn)
    //     - flutter_texture_id      (int64_t Flutter ID)
    //     - gl_name                 (0 — GL init is deferred to populate())
    //
    // Under the new deferred-GL design, create_gl_texture registers the texture
    // with FlTextureRegistrar and kicks populate() via mark_texture_frame_available,
    // but does NOT call glGenTextures / glTexImage2D yet.  Those GL calls happen
    // inside populate() on Flutter's raster thread where a GL context is current.
    // cudaGraphicsGLRegisterImage likewise happens there.  We therefore accept
    // gl_name == 0 here; the actual CUDA resource handle is obtained later via
    // wait_initialized().
    void*    texture_object  = nullptr;
    int64_t  texture_id      = -1;
    uint32_t gl_name         = 0;

    int rc = ops.create_gl_texture(registrar, width, height,
                                   &texture_object, &texture_id, &gl_name);
    if (rc != 0 || !texture_object || texture_id < 0) {
        flow_set_error(FLOW_ERROR_UNKNOWN,
                       "flow_ffi_register_cuda_flutter_texture: "
                       "create_gl_texture callback failed");
        return FLOW_ERROR_UNKNOWN;
    }

    // (6) Build the state object and insert it into the registry.
    //     cuda_resource starts null; it is filled in by wait_initialized()
    //     before write_into() uses it.
    auto state = std::make_unique<CudaTextureState>();
    state->gl_texture_name        = gl_name;   // 0 until populate() fires
    state->cuda_resource          = nullptr;   // set by wait_initialized()
    state->flutter_texture_id     = texture_id;
    state->width                  = width;
    state->height                 = height;
    state->flutter_texture_object = texture_object;
    state->stream                 = stream_guard.release();

    FlowCudaTextureHandle handle = static_cast<FlowCudaTextureHandle>(state.get());

    {
        std::lock_guard<std::mutex> lk(g_registry_mutex);
        g_registry.emplace(handle, std::move(state));
    }

    *out_handle             = handle;
    *out_flutter_texture_id = texture_id;
    return FLOW_SUCCESS;
}

// ---------------------------------------------------------------------------

extern "C" FlowError flow_ffi_unregister_cuda_flutter_texture(
    FlowCudaTextureHandle handle)
{
    if (!handle) {
        flow_set_error(FLOW_ERROR_INVALID_HANDLE,
                       "flow_ffi_unregister_cuda_flutter_texture: null handle");
        return FLOW_ERROR_INVALID_HANDLE;
    }

    std::unique_ptr<CudaTextureState> state;
    {
        std::lock_guard<std::mutex> lk(g_registry_mutex);
        auto it = g_registry.find(handle);
        if (it == g_registry.end()) {
            flow_set_error(FLOW_ERROR_INVALID_HANDLE,
                           "flow_ffi_unregister_cuda_flutter_texture: "
                           "handle not found in registry");
            return FLOW_ERROR_INVALID_HANDLE;
        }
        state = std::move(it->second);
        g_registry.erase(it);
    }

    // Synchronise the per-texture stream before tearing down resources.
    if (state->stream) {
        cudaStreamSynchronize(state->stream);
    }

    // Unregister from CUDA.
    if (state->cuda_resource) {
        cudaError_t e = cudaGraphicsUnregisterResource(state->cuda_resource);
        if (e != cudaSuccess) {
            cuda_log_error("cudaGraphicsUnregisterResource", e);
            // Continue cleanup — leaking the CUDA resource is better than
            // leaving the Flutter texture registered.
        }
    }

    // Tell Flutter this texture is going away and release the GObject ref.
    if (state->flutter_texture_object) {
        if (flow_ffi_is_texture_ops_bound()) {
            FlowTextureOps ops = flow_ffi_get_texture_ops();
            void* registrar    = flow_ffi_get_texture_registrar();
            ops.destroy_gl_texture(registrar,
                                   state->flutter_texture_object,
                                   state->flutter_texture_id);
        }
        // If ops have been torn down (e.g. plugin unloaded), skip the call
        // to avoid a null-ptr dispatch; the Flutter engine cleans up on exit.
    }

    // Destroy CUDA stream.
    if (state->stream) {
        cudaStreamDestroy(state->stream);
    }

    return FLOW_SUCCESS;
}

// ---------------------------------------------------------------------------

extern "C" FlowError flow_ffi_cuda_write_into(
    FlowCudaTextureHandle handle,
    const void*           src_device_ptr,
    int32_t               src_pitch_bytes,
    int32_t               width,
    int32_t               height,
    void*                 producer_ready_event)
{
    if (!handle) {
        return FLOW_ERROR_INVALID_HANDLE;
    }
    if (!src_device_ptr || src_pitch_bytes <= 0 || width <= 0 || height <= 0) {
        return FLOW_ERROR_INVALID_ARGUMENT;
    }

    // Look up state and snapshot the fields we need (holding mutex briefly).
    void* tex_obj = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_registry_mutex);
        CudaTextureState* s = find_state_locked(handle);
        if (!s) {
            flow_set_error(FLOW_ERROR_INVALID_HANDLE,
                           "flow_ffi_cuda_write_into: handle not in registry");
            return FLOW_ERROR_INVALID_HANDLE;
        }
        // Validate dimensions match registered texture.
        if (width != s->width || height != s->height) {
            flow_set_error(FLOW_ERROR_INVALID_ARGUMENT,
                           "flow_ffi_cuda_write_into: dimensions mismatch — "
                           "unregister and re-register on resize");
            return FLOW_ERROR_INVALID_ARGUMENT;
        }
        tex_obj = s->flutter_texture_object;
    }
    // Mutex released.

    if (!flow_ffi_is_texture_ops_bound()) {
        return FLOW_SUCCESS; // ops torn down, nothing to do
    }
    FlowTextureOps ops = flow_ffi_get_texture_ops();

    // Wait for the raster thread to complete GL+CUDA init (one-time cost,
    // ~1 vsync on first frame).  After this rendezvous the staging buffer is
    // allocated and submit_frame is safe to call.
    {
        void* dummy_cuda_res = nullptr;
        int wait_rc = ops.wait_initialized(tex_obj, &dummy_cuda_res);
        if (wait_rc != 0) {
            // Init failed or timed out — drop this frame silently.
            return FLOW_SUCCESS;
        }
    }

    // Delegate the copy to submit_frame, which performs a pure D→D
    // cudaMemcpy2DAsync into the staging buffer — no GL context required.
    // populate() on the raster thread does the GL-coupled Map/MemcpyToArray/
    // Unmap at the next vsync.
    int submit_rc = ops.submit_frame(
        tex_obj,
        src_device_ptr,
        src_pitch_bytes,
        width, height,
        producer_ready_event);
    if (submit_rc != 0) {
        // Frame dropped (dim mismatch, staging not ready, CUDA error).
        // Keep cascade moving — this is not a fatal condition.
        return FLOW_SUCCESS;
    }

    return FLOW_SUCCESS;
}

// ---------------------------------------------------------------------------

extern "C" void flow_ffi_signal_frame_available(int64_t flutter_texture_id)
{
    if (!flow_ffi_is_texture_ops_bound() || !flow_ffi_is_texture_registrar_bound()) {
        return;
    }

    // Find the texture object whose Flutter ID matches.
    void* texture_object = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_registry_mutex);
        for (auto& kv : g_registry) {
            if (kv.second->flutter_texture_id == flutter_texture_id) {
                texture_object = kv.second->flutter_texture_object;
                break;
            }
        }
    }

    if (!texture_object) {
        return;  // Not found — caller may have already unregistered.
    }

    FlowTextureOps ops  = flow_ffi_get_texture_ops();
    void*   registrar   = flow_ffi_get_texture_registrar();
    ops.mark_frame_available(registrar, texture_object);
}
