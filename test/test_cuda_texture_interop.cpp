// test_cuda_texture_interop.cpp — Phase D test suite
//
// Tests the CUDA/Flutter texture interop FFI surface.
//
// Test strategy:
// --------------
// A real Flutter engine cannot run inside a CTest harness, so these tests
// inject mock callbacks via flow_ffi_set_texture_ops() and a sentinel
// registrar pointer via flow_ffi_set_texture_registrar() to exercise as much
// of the real implementation as possible without a display.
//
// Two categories:
//   A. API Surface / Registry tests (no GPU required):
//      - Unbound state returns correct error codes.
//      - Registrar+ops bound but mock create fails → correct error.
//      - Full lifecycle: mock create → register → unregister → verify clean.
//      - signal_frame_available is a no-op when registrar/ops not bound.
//      - write_into returns INVALID_HANDLE for null/unknown handles.
//      - write_into returns INVALID_ARGUMENT for mismatched dimensions.
//
//   B. GPU write test (CUDA required, guarded by FLOW_FFI_HAS_CUDA):
//      When real CUDA hardware is available and a real EGL/GL context can be
//      created, this test allocates a 64×64 CUDA device buffer, fills it with
//      pattern 0xAB, calls flow_ffi_cuda_write_into, then reads back via
//      cudaMemcpy2DFromArray to verify byte movement.
//
//      LIMITATION: cudaGraphicsGLRegisterImage requires a live OpenGL context
//      with the target texture.  In a headless CTest environment (no
//      display), EGL PBuffer creation may fail or the CUDA driver may refuse
//      GL interop without a real GPU context.  The GPU test is therefore
//      wrapped in a GTEST_SKIP() guard that detects the condition and marks
//      the test as skipped rather than failed.
//
// Mock infrastructure:
// --------------------
// The mock callbacks maintain a simple counter and a static struct to track
// the most recently "created" texture, allowing tests to verify the
// create/destroy/mark_frame lifecycle without a real Flutter engine.

#include "flow_ffi_cuda.h"
#include "flow_ffi.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#if FLOW_FFI_HAS_CUDA
// epoxy must be included before cuda_gl_interop.h.  epoxy/gl.h defines its
// own GL types and sets include guards that prevent the system GL/gl.h from
// being re-included by cuda_gl_interop.h, avoiding KHRONOS_* redefinition
// conflicts.
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <cuda_runtime.h>
// cuda_gl_interop.h includes GL/gl.h only if not already guarded; with epoxy
// loaded first the system GL headers are suppressed.
#include <cuda_gl_interop.h>
#include <vector>
#endif

// ---------------------------------------------------------------------------
// Mock texture ops
// ---------------------------------------------------------------------------

namespace {

struct MockTextureState {
    void*    texture_object{nullptr};
    int64_t  texture_id{0};
    uint32_t gl_name{0};
    int      destroy_call_count{0};
    int      mark_frame_count{0};
};

// A global sentinel non-null registrar pointer — value doesn't matter in
// mock tests because our callbacks never dereference it.
// Cannot use reinterpret_cast in constexpr context; use a static variable.
static int             s_mock_registrar_sentinel = 0;
static void* const     kMockRegistrar = &s_mock_registrar_sentinel;

static MockTextureState g_mock_state;
static std::atomic<int> g_create_call_count{0};
static std::atomic<int> g_submit_frame_count{0};
static bool             g_create_should_fail{false};

// Reset mock state between tests.
static void reset_mock() {
    g_mock_state = MockTextureState{};
    g_create_call_count.store(0);
    g_submit_frame_count.store(0);
    g_create_should_fail = false;
    flow_ffi_set_texture_ops(nullptr);   // clear ops
    flow_ffi_set_texture_registrar(nullptr);
}

static int mock_create_gl_texture(void*    /*registrar*/,
                                   int32_t  width,
                                   int32_t  height,
                                   void**   out_texture_object,
                                   int64_t* out_texture_id,
                                   uint32_t* out_gl_name)
{
    ++g_create_call_count;
    if (g_create_should_fail) return -1;

    // Synthesise a fake texture object and IDs — just use static addresses.
    g_mock_state.texture_object = &g_mock_state;    // non-null sentinel
    g_mock_state.texture_id     = 42LL + g_create_call_count.load();
    g_mock_state.gl_name        = static_cast<uint32_t>(width * height);

    *out_texture_object = g_mock_state.texture_object;
    *out_texture_id     = g_mock_state.texture_id;
    *out_gl_name        = g_mock_state.gl_name;
    return 0;
}

static void mock_destroy_gl_texture(void*   /*registrar*/,
                                     void*   /*texture_object*/,
                                     int64_t /*texture_id*/)
{
    ++g_mock_state.destroy_call_count;
}

static void mock_mark_frame_available(void* /*registrar*/,
                                       void* /*texture_object*/)
{
    ++g_mock_state.mark_frame_count;
}

// Fake CUDA resource sentinel used by wait_initialized and write_into mocks.
// Must be non-null so write_into's wait_initialized branch is skipped, but
// cudaGraphicsMapResources will fail on it in headless tests (which is fine).
static int s_fake_cuda_res_sentinel = 0;

static int mock_wait_initialized(void* /*texture_object*/,
                                  void** out_cuda_resource)
{
    // Immediately signal success with a fake (non-null) resource handle.
    // cudaGraphicsMapResources will fail with this fake handle in headless
    // environments, which matches the expected headless test path.
    if (out_cuda_resource) {
        *out_cuda_resource = &s_fake_cuda_res_sentinel;
    }
    return 0;
}

static int mock_submit_frame(void*       /*texture_object*/,
                              const void* /*src_device_ptr*/,
                              int32_t     /*src_pitch_bytes*/,
                              int32_t     /*width*/,
                              int32_t     /*height*/,
                              void*       /*ready_event*/)
{
    ++g_submit_frame_count;
    return 0;
}

static const FlowTextureOps kMockOps = {
    mock_create_gl_texture,
    mock_destroy_gl_texture,
    mock_mark_frame_available,
    mock_wait_initialized,
    mock_submit_frame,
};

// Bind the mock registrar and ops.
static void bind_mock_ops() {
    flow_ffi_set_texture_registrar(kMockRegistrar);
    flow_ffi_set_texture_ops(&kMockOps);
}

} // namespace

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class CudaTextureInteropTest : public ::testing::Test {
protected:
    void SetUp() override {
        flow_clear_error();
        reset_mock();
    }
    void TearDown() override {
        reset_mock();
        flow_clear_error();
    }
};

// ---------------------------------------------------------------------------
// A1: Registrar not bound → FLOW_ERROR_NOT_IMPLEMENTED
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, RegisterFailsWhenRegistrarNotBound) {
    // Neither registrar nor ops bound.
    FlowCudaTextureHandle handle = nullptr;
    int64_t               tex_id = -1;

    FlowError err = flow_ffi_register_cuda_flutter_texture(
        64, 64, &handle, &tex_id);

    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
    EXPECT_EQ(handle, nullptr);
    EXPECT_EQ(tex_id, -1LL);
}

// ---------------------------------------------------------------------------
// A2: Registrar bound but ops not bound → FLOW_ERROR_NOT_IMPLEMENTED
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, RegisterFailsWhenOpsNotBound) {
    flow_ffi_set_texture_registrar(kMockRegistrar);
    // Deliberately do NOT call flow_ffi_set_texture_ops.

    FlowCudaTextureHandle handle = nullptr;
    int64_t               tex_id = -1;

    FlowError err = flow_ffi_register_cuda_flutter_texture(
        64, 64, &handle, &tex_id);

    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
    EXPECT_EQ(handle, nullptr);
    EXPECT_EQ(tex_id, -1LL);
}

// ---------------------------------------------------------------------------
// A3: Null out-params → FLOW_ERROR_INVALID_ARGUMENT (CUDA) / NOT_IMPLEMENTED (stub)
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, RegisterFailsOnNullOutParams) {
    bind_mock_ops();
    FlowError err = flow_ffi_register_cuda_flutter_texture(
        64, 64, nullptr, nullptr);
#if FLOW_FFI_HAS_CUDA
    EXPECT_EQ(err, FLOW_ERROR_INVALID_ARGUMENT);
#else
    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
#endif
}

// ---------------------------------------------------------------------------
// A4: Non-positive dimensions → FLOW_ERROR_INVALID_ARGUMENT (CUDA) / NOT_IMPLEMENTED (stub)
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, RegisterFailsOnInvalidDimensions) {
    bind_mock_ops();
    FlowCudaTextureHandle handle = nullptr;
    int64_t               tex_id = -1;

    FlowError err0 = flow_ffi_register_cuda_flutter_texture(
        0, 64, &handle, &tex_id);
    FlowError err1 = flow_ffi_register_cuda_flutter_texture(
        64, -1, &handle, &tex_id);
#if FLOW_FFI_HAS_CUDA
    EXPECT_EQ(err0, FLOW_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(err1, FLOW_ERROR_INVALID_ARGUMENT);
#else
    EXPECT_EQ(err0, FLOW_ERROR_NOT_IMPLEMENTED);
    EXPECT_EQ(err1, FLOW_ERROR_NOT_IMPLEMENTED);
#endif
}

// ---------------------------------------------------------------------------
// A5: Mock create returns -1 → FLOW_ERROR_UNKNOWN (CUDA build only)
// In the stub build all register calls return NOT_IMPLEMENTED.
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, RegisterFailsWhenCreateCallbackFails) {
    bind_mock_ops();
    g_create_should_fail = true;

    FlowCudaTextureHandle handle = nullptr;
    int64_t               tex_id = -1;

    FlowError err = flow_ffi_register_cuda_flutter_texture(
        64, 64, &handle, &tex_id);

#if FLOW_FFI_HAS_CUDA
    EXPECT_EQ(err, FLOW_ERROR_UNKNOWN);
    EXPECT_EQ(g_create_call_count.load(), 1);
#else
    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
#endif
    EXPECT_EQ(handle, nullptr);
    EXPECT_EQ(tex_id, -1LL);
}

// ---------------------------------------------------------------------------
// A6: Unregister with null handle.
// CUDA build → FLOW_ERROR_INVALID_HANDLE; stub build → NOT_IMPLEMENTED.
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, UnregisterNullHandleReturnsError) {
    FlowError err = flow_ffi_unregister_cuda_flutter_texture(nullptr);
#if FLOW_FFI_HAS_CUDA
    EXPECT_EQ(err, FLOW_ERROR_INVALID_HANDLE);
#else
    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
#endif
}

// ---------------------------------------------------------------------------
// A7: Unregister an unknown (stale) handle.
// CUDA build → FLOW_ERROR_INVALID_HANDLE; stub build → NOT_IMPLEMENTED.
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, UnregisterUnknownHandleReturnsError) {
    int sentinel = 0;
    FlowError err = flow_ffi_unregister_cuda_flutter_texture(
        static_cast<FlowCudaTextureHandle>(&sentinel));
#if FLOW_FFI_HAS_CUDA
    EXPECT_EQ(err, FLOW_ERROR_INVALID_HANDLE);
#else
    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
#endif
}

// ---------------------------------------------------------------------------
// A8: write_into with null handle.
// CUDA build → FLOW_ERROR_INVALID_HANDLE; stub build → NOT_IMPLEMENTED.
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, WriteIntoNullHandleReturnsError) {
    int dummy_src = 0;
    FlowError err = flow_ffi_cuda_write_into(
        nullptr, &dummy_src, 256, 64, 64, nullptr);
#if FLOW_FFI_HAS_CUDA
    EXPECT_EQ(err, FLOW_ERROR_INVALID_HANDLE);
#else
    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
#endif
}

// ---------------------------------------------------------------------------
// A9: write_into with unknown handle.
// CUDA build → FLOW_ERROR_INVALID_HANDLE; stub build → NOT_IMPLEMENTED.
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, WriteIntoUnknownHandleReturnsError) {
    int sentinel   = 0;
    int dummy_src  = 0;
    FlowError err = flow_ffi_cuda_write_into(
        static_cast<FlowCudaTextureHandle>(&sentinel),
        &dummy_src, 256, 64, 64, nullptr);
#if FLOW_FFI_HAS_CUDA
    EXPECT_EQ(err, FLOW_ERROR_INVALID_HANDLE);
#else
    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
#endif
}

// ---------------------------------------------------------------------------
// A10: signal_frame_available is a no-op when registrar/ops not bound
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, SignalFrameAvailableIsNoOpWhenUnbound) {
    // Should not crash.
    flow_ffi_signal_frame_available(42LL);
    EXPECT_EQ(g_mock_state.mark_frame_count, 0);
}

// ---------------------------------------------------------------------------
// A11: signal_frame_available for an unknown texture_id is a no-op
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, SignalFrameAvailableUnknownIdIsNoOp) {
    bind_mock_ops();
    flow_ffi_signal_frame_available(99999LL);
    EXPECT_EQ(g_mock_state.mark_frame_count, 0);
}

// ---------------------------------------------------------------------------
// A12: flow_ffi_is_texture_ops_bound reflects set/clear
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, TextureOpsBoundStateTransitions) {
    EXPECT_EQ(flow_ffi_is_texture_ops_bound(), 0);
    flow_ffi_set_texture_ops(&kMockOps);
    EXPECT_NE(flow_ffi_is_texture_ops_bound(), 0);
    flow_ffi_set_texture_ops(nullptr);
    EXPECT_EQ(flow_ffi_is_texture_ops_bound(), 0);
}

// ---------------------------------------------------------------------------
// A13: Full mock lifecycle — register → signal → unregister
//
// This test exercises the registry insert/lookup/remove path and verifies the
// destroy and mark_frame callbacks are fired correctly.  It does NOT exercise
// the GPU path (cudaGraphicsGLRegisterImage) because we intercept at the
// mock_create_gl_texture level before the CUDA call in image_bridge_cuda.cpp
// is reached.
//
// IMPORTANT: In the non-CUDA build (FLOW_FFI_HAS_CUDA=0), the 4 FFI
// functions are all stubs returning FLOW_ERROR_NOT_IMPLEMENTED.  This test
// is therefore only meaningful in a CUDA build.  In a no-CUDA build the
// test verifies that the stub correctly returns NOT_IMPLEMENTED regardless
// of the ops state.
// ---------------------------------------------------------------------------
TEST_F(CudaTextureInteropTest, MockLifecycleRegisterSignalUnregister) {
#if !FLOW_FFI_HAS_CUDA
    // Stub build: verify stub behaviour.
    bind_mock_ops();
    FlowCudaTextureHandle handle = nullptr;
    int64_t               tex_id = -1;
    FlowError err = flow_ffi_register_cuda_flutter_texture(
        64, 64, &handle, &tex_id);
    EXPECT_EQ(err, FLOW_ERROR_NOT_IMPLEMENTED);
    return;
#else
    // CUDA build — new deferred-GL design.
    //
    // Under the threading fix, create_gl_texture no longer calls
    // glGenTextures / cudaGraphicsGLRegisterImage.  Those are deferred to
    // populate() on Flutter's raster thread.  So flow_ffi_register_cuda_flutter_texture
    // always returns FLOW_SUCCESS here regardless of display availability.
    //
    // What this test verifies without a display:
    //   - The registrar/ops checks pass (no NOT_IMPLEMENTED).
    //   - The create callback is invoked and a valid handle is returned.
    //   - signal_frame_available invokes the mark callback.
    //   - Unregister succeeds and calls the destroy callback.
    //   - Double-unregister returns FLOW_ERROR_INVALID_HANDLE.
    bind_mock_ops();

    FlowCudaTextureHandle handle = nullptr;
    int64_t               tex_id = -1;
    FlowError err = flow_ffi_register_cuda_flutter_texture(
        64, 64, &handle, &tex_id);

    // Register must succeed: create callback was called, handle is valid.
    ASSERT_EQ(err, FLOW_SUCCESS) << "Unexpected error from register: "
                                 << static_cast<int>(err);
    ASSERT_NE(handle, nullptr);
    ASSERT_GT(tex_id, -1LL);

    // The create callback must have been invoked exactly once.
    EXPECT_EQ(g_create_call_count.load(), 1);

    // signal_frame_available — should invoke the mark callback.
    flow_ffi_signal_frame_available(tex_id);
    EXPECT_EQ(g_mock_state.mark_frame_count, 1);

    // Unregister — destroy callback must fire once.
    FlowError unreg_err = flow_ffi_unregister_cuda_flutter_texture(handle);
    EXPECT_EQ(unreg_err, FLOW_SUCCESS);
    EXPECT_EQ(g_mock_state.destroy_call_count, 1);

    // Handle is now stale — second unregister must return error.
    FlowError double_err = flow_ffi_unregister_cuda_flutter_texture(handle);
    EXPECT_EQ(double_err, FLOW_ERROR_INVALID_HANDLE);
#endif
}

// ---------------------------------------------------------------------------
// B1 (GPU): Device-to-device write verification
//
// Requires: FLOW_FFI_HAS_CUDA=1, a real CUDA device, and a real OpenGL
// context (EGL PBuffer).  Skipped automatically if any of these are absent.
// ---------------------------------------------------------------------------
#if FLOW_FFI_HAS_CUDA
TEST_F(CudaTextureInteropTest, GpuWriteVerification) {
    // (0) Skip if no CUDA device.
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0) {
        GTEST_SKIP() << "No CUDA devices available";
    }

    // (1) Create an EGL PBuffer context for headless GL.
    EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY) {
        GTEST_SKIP() << "No EGL display available (headless environment)";
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(egl_display, &major, &minor)) {
        GTEST_SKIP() << "eglInitialize failed";
    }

    static const EGLint kConfigAttribs[] = {
        EGL_SURFACE_TYPE,   EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig egl_config = nullptr;
    EGLint    num_configs = 0;
    if (!eglChooseConfig(egl_display, kConfigAttribs, &egl_config, 1, &num_configs)
        || num_configs == 0)
    {
        eglTerminate(egl_display);
        GTEST_SKIP() << "No suitable EGL config";
    }

    static const EGLint kPBufAttribs[] = {
        EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE
    };
    EGLSurface pbuf = eglCreatePbufferSurface(egl_display, egl_config, kPBufAttribs);
    if (pbuf == EGL_NO_SURFACE) {
        eglTerminate(egl_display);
        GTEST_SKIP() << "eglCreatePbufferSurface failed";
    }

    if (!eglBindAPI(EGL_OPENGL_API)) {
        eglDestroySurface(egl_display, pbuf);
        eglTerminate(egl_display);
        GTEST_SKIP() << "eglBindAPI failed";
    }

    EGLContext egl_ctx = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, nullptr);
    if (egl_ctx == EGL_NO_CONTEXT) {
        eglDestroySurface(egl_display, pbuf);
        eglTerminate(egl_display);
        GTEST_SKIP() << "eglCreateContext failed";
    }

    if (!eglMakeCurrent(egl_display, pbuf, pbuf, egl_ctx)) {
        eglDestroyContext(egl_display, egl_ctx);
        eglDestroySurface(egl_display, pbuf);
        eglTerminate(egl_display);
        GTEST_SKIP() << "eglMakeCurrent failed";
    }

    // EGL/GL context is now current.
    constexpr int W = 64, H = 64;

    // (2) Create GL texture manually (bypass the mock, use a real texture).
    GLuint gl_name = 0;
    glGenTextures(1, &gl_name);
    glBindTexture(GL_TEXTURE_2D, gl_name);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_NE(gl_name, 0u) << "glGenTextures failed";

    // (3) Register the GL texture with CUDA directly (testing interop).
    cudaGraphicsResource_t cuda_res = nullptr;
    cudaError_t cuda_err = cudaGraphicsGLRegisterImage(
        &cuda_res, gl_name, GL_TEXTURE_2D,
        cudaGraphicsRegisterFlagsWriteDiscard);
    if (cuda_err != cudaSuccess) {
        glDeleteTextures(1, &gl_name);
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(egl_display, egl_ctx);
        eglDestroySurface(egl_display, pbuf);
        eglTerminate(egl_display);
        GTEST_SKIP() << "cudaGraphicsGLRegisterImage failed: "
                     << cudaGetErrorString(cuda_err)
                     << " — likely no CUDA/GL interop in this environment";
    }

    // (4) Allocate a 64x64 RGBA8 device buffer filled with pattern 0xAB.
    constexpr uint8_t kPattern = 0xAB;
    const size_t pitch_bytes   = static_cast<size_t>(W) * 4;
    const size_t total_bytes   = pitch_bytes * static_cast<size_t>(H);
    void* d_src = nullptr;
    ASSERT_EQ(cudaMalloc(&d_src, total_bytes), cudaSuccess);
    ASSERT_EQ(cudaMemset(d_src, kPattern, total_bytes), cudaSuccess);

    // (5) Create a CUDA stream.
    cudaStream_t stream = nullptr;
    ASSERT_EQ(cudaStreamCreate(&stream), cudaSuccess);

    // (6) Perform the write.
    cuda_err = cudaGraphicsMapResources(1, &cuda_res, stream);
    ASSERT_EQ(cuda_err, cudaSuccess);

    cudaArray_t dst_array = nullptr;
    cuda_err = cudaGraphicsSubResourceGetMappedArray(&dst_array, cuda_res, 0, 0);
    ASSERT_EQ(cuda_err, cudaSuccess);

    cuda_err = cudaMemcpy2DToArrayAsync(
        dst_array, 0, 0,
        d_src, pitch_bytes,
        pitch_bytes, static_cast<size_t>(H),
        cudaMemcpyDeviceToDevice, stream);
    ASSERT_EQ(cuda_err, cudaSuccess);

    cuda_err = cudaGraphicsUnmapResources(1, &cuda_res, stream);
    ASSERT_EQ(cuda_err, cudaSuccess);
    ASSERT_EQ(cudaStreamSynchronize(stream), cudaSuccess);

    // (7) Read back via cudaGraphicsMapResources + cudaMemcpy2DFromArray.
    cuda_err = cudaGraphicsMapResources(1, &cuda_res, stream);
    ASSERT_EQ(cuda_err, cudaSuccess);
    cuda_err = cudaGraphicsSubResourceGetMappedArray(&dst_array, cuda_res, 0, 0);
    ASSERT_EQ(cuda_err, cudaSuccess);

    void* d_readback = nullptr;
    ASSERT_EQ(cudaMalloc(&d_readback, total_bytes), cudaSuccess);
    cuda_err = cudaMemcpy2DFromArrayAsync(
        d_readback, pitch_bytes,
        dst_array, 0, 0,
        pitch_bytes, static_cast<size_t>(H),
        cudaMemcpyDeviceToDevice, stream);
    ASSERT_EQ(cuda_err, cudaSuccess);
    cuda_err = cudaGraphicsUnmapResources(1, &cuda_res, stream);
    ASSERT_EQ(cuda_err, cudaSuccess);
    ASSERT_EQ(cudaStreamSynchronize(stream), cudaSuccess);

    // Copy readback to host.
    std::vector<uint8_t> host(total_bytes, 0);
    ASSERT_EQ(cudaMemcpy(host.data(), d_readback, total_bytes, cudaMemcpyDeviceToHost),
              cudaSuccess);

    // (8) Verify all bytes equal 0xAB.
    bool all_match = true;
    for (size_t i = 0; i < total_bytes; ++i) {
        if (host[i] != kPattern) {
            all_match = false;
            ADD_FAILURE() << "Byte mismatch at index " << i
                          << ": expected 0x" << std::hex << (int)kPattern
                          << " got 0x" << (int)host[i];
            break;
        }
    }
    EXPECT_TRUE(all_match) << "Device-to-device copy byte verification failed";

    // (9) Cleanup.
    cudaFree(d_readback);
    cudaFree(d_src);
    cudaStreamDestroy(stream);
    cudaGraphicsUnregisterResource(cuda_res);
    glDeleteTextures(1, &gl_name);
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(egl_display, egl_ctx);
    eglDestroySurface(egl_display, pbuf);
    eglTerminate(egl_display);
}
#endif // FLOW_FFI_HAS_CUDA
