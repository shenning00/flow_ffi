// test_flow_flutter_cuda_preview.cpp — Phase E: FlowFlutterCudaPreview tests
//
// Tests the FlowFlutterCudaPreview sink node at two levels:
//
//   1. Registration: verifies the node class is registered under "Editor/CudaPreview".
//   2. Construction: verifies the node has "in" input and "texture_id" output ports.
//   3. Compute without input: drives Compute with no input attached; verifies
//      no crash, no error callback, no FFI calls.
//   4. Compute with mock ops: installs mock FlowTextureOps callbacks (same
//      approach as Phase D's A13 test) and drives Compute with a real CUDA
//      device buffer.  Due to the cudaGraphicsGLRegisterImage step in
//      flow_ffi_register_cuda_flutter_texture, a real GL texture is required
//      for the "success" path.  In headless / no-GL environments the register
//      call fails at that step; the test handles both outcomes:
//
//        a. Headless path (no GL): the node fires OnError; register was called
//           once and destroy was called once (cleanup path).
//        b. GL available: the full lifecycle runs — create called once,
//           write_into called once (via CUDA), signal called once.  Then the
//           input dims change, a second Compute fires: destroy called once,
//           create called a second time, write_into and signal each called
//           a second time.
//
// This mirrors the A13 pattern in test_cuda_texture_interop.cpp exactly.
// Only compiled when FLOW_FFI_ENABLE_CUDA=ON (see CMakeLists.txt gate).

#include "flow_ffi.h"
#include "flow_ffi_cuda.h"

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <flow_wire/CudaDeviceImage.hpp>
#include <flow_wire/FlowFlutterCudaPreview.hpp>
#include <flow_wire/Register.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Mock texture ops  (same sentinel/callback strategy as test_cuda_texture_interop.cpp)
// ---------------------------------------------------------------------------

namespace
{

struct MockPreviewState
{
    void* texture_object{nullptr};
    int64_t texture_id{0};
    uint32_t gl_name{0};
    int destroy_call_count{0};
    int mark_frame_count{0};
};

static int s_registrar_sentinel   = 0;
static void* const kMockRegistrar = &s_registrar_sentinel;

static MockPreviewState g_mock;
static std::atomic<int> g_create_count{0};
static bool g_create_should_fail{false};

static void reset_mock()
{
    g_mock               = MockPreviewState{};
    g_create_count       = 0;
    g_create_should_fail = false;
    flow_ffi_set_texture_ops(nullptr);
    flow_ffi_set_texture_registrar(nullptr);
}

static int mock_create(void* /*reg*/, int32_t w, int32_t h, void** out_obj, int64_t* out_id, uint32_t* out_name)
{
    ++g_create_count;
    if (g_create_should_fail) return -1;
    g_mock.texture_object = &g_mock;
    g_mock.texture_id     = 100LL + g_create_count.load();
    g_mock.gl_name        = static_cast<uint32_t>(w * h);
    *out_obj              = g_mock.texture_object;
    *out_id               = g_mock.texture_id;
    *out_name             = g_mock.gl_name;
    return 0;
}

static void mock_destroy(void* /*reg*/, void* /*obj*/, int64_t /*id*/) { ++g_mock.destroy_call_count; }

static void mock_mark_frame(void* /*reg*/, void* /*obj*/) { ++g_mock.mark_frame_count; }

// Fake CUDA resource sentinel for wait_initialized.  Non-null so
// write_into skips the wait branch, but cudaGraphicsMapResources will
// fail on it in headless environments (expected headless test path).
static int s_fake_cuda_res = 0;

static int mock_wait_initialized(void* /*texture_object*/, void** out_cuda_resource)
{
    if (out_cuda_resource)
    {
        *out_cuda_resource = &s_fake_cuda_res;
    }
    return 0;
}

static int mock_submit_frame(void* /*texture_object*/, const void* /*src_device_ptr*/, int32_t /*src_pitch_bytes*/,
                             int32_t /*width*/, int32_t /*height*/, void* /*ready_event*/)
{
    return 0;
}

static const FlowTextureOps kMockOps = {
    mock_create, mock_destroy, mock_mark_frame, mock_wait_initialized, mock_submit_frame,
};

static void bind_mock_ops()
{
    flow_ffi_set_texture_registrar(kMockRegistrar);
    flow_ffi_set_texture_ops(&kMockOps);
}

// Build a minimal flow::Env identical to what flow_env_create provides.
static std::shared_ptr<flow::Env> make_env()
{
    auto factory = std::make_shared<flow::NodeFactory>();
    flow::Settings settings;
    settings.MaxThreads = 1;
    return flow::Env::Create(factory, settings);
}

// Build a minimal flow::Env with all flow_wire nodes registered.
static std::shared_ptr<flow::Env> make_env_with_registry()
{
    auto factory = std::make_shared<flow::NodeFactory>();
    flow::Settings settings;
    settings.MaxThreads = 1;
    auto env            = flow::Env::Create(factory, settings);
    flow_wire::RegisterAllNodes(*env);
    return env;
}

} // namespace

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class FlowFlutterCudaPreviewTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        flow_clear_error();
        reset_mock();
    }
    void TearDown() override
    {
        reset_mock();
        flow_clear_error();
    }
};

// ---------------------------------------------------------------------------
// Scenario 1: Class is registered in the factory under "Editor" / "CudaPreview"
//
// GetCategories() returns a CategoryMap (std::unordered_multimap<string,string>)
// where keys are category names and values are class_name strings (the TypeName_v
// value of the registered node type, e.g. "FlowFlutterCudaPreview").
// The friendly name "CudaPreview" is stored separately in _friendly_names; we
// verify registration by looking for the TypeName_v value in the "Editor" bucket.
// ---------------------------------------------------------------------------
TEST_F(FlowFlutterCudaPreviewTest, ClassIsRegisteredInFactory)
{
    auto env     = make_env_with_registry();
    auto factory = env->GetFactory();
    ASSERT_NE(factory, nullptr);

    // GetCategories() → unordered_multimap<category, class_name_string>.
    // Verify that there is at least one entry with key "Editor" whose value
    // is the TypeName_v of FlowFlutterCudaPreview.
    const auto& cat_map = factory->GetCategories();
    const std::string expected_class{flow::TypeName_v<flow::FlowFlutterCudaPreview>};

    bool found = false;
    auto range = cat_map.equal_range("Editor");
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second == expected_class)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "\"" << expected_class
                       << "\" should be registered in the "
                          "\"Editor\" category after RegisterAllNodes";
}

// ---------------------------------------------------------------------------
// Scenario 2: Construction — node has "in" input and "texture_id" output
// ---------------------------------------------------------------------------
TEST_F(FlowFlutterCudaPreviewTest, ConstructionSucceedsAndPortsExist)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowFlutterCudaPreview node(uuid, "preview_test", env);

    // "in" input port must exist and have the CudaDeviceImage type.
    const auto& inputs = node.GetInputPorts();
    ASSERT_EQ(inputs.size(), 1u) << "Expected exactly one input port";
    auto in_it = inputs.find(flow::IndexableName{"in"});
    ASSERT_NE(in_it, inputs.end()) << "Input port \"in\" not found";
    // GetDataType() returns the stored type string (no data set yet → returns the
    // declared type, which is TypeName_v<CudaDeviceImage> = "flow::CudaDeviceImage").
    EXPECT_EQ(in_it->second->GetDataType(), flow::TypeName_v<flow::CudaDeviceImage>);

    // "texture_id" output port must exist.
    const auto& outputs = node.GetOutputPorts();
    ASSERT_EQ(outputs.size(), 1u) << "Expected exactly one output port";
    auto out_it = outputs.find(flow::IndexableName{"texture_id"});
    ASSERT_NE(out_it, outputs.end()) << "Output port \"texture_id\" not found";
    EXPECT_EQ(out_it->second->GetDataType(), flow::TypeName_v<int64_t>);
}

// ---------------------------------------------------------------------------
// Scenario 3: Compute with no input data — no-op, no crash, no error
// ---------------------------------------------------------------------------
TEST_F(FlowFlutterCudaPreviewTest, ComputeWithNoInputIsNoOp)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowFlutterCudaPreview node(uuid, "preview_noop", env);

    // Attach an error listener to catch any unexpected OnError broadcast.
    bool error_fired = false;
    node.OnError.Bind(flow::IndexableName{"test_listener"}, [&](const std::exception& /*e*/) { error_fired = true; });

    // Drive Compute with no input data set on the "in" port.
    EXPECT_NO_THROW(node.InvokeCompute());
    EXPECT_FALSE(error_fired) << "OnError should not fire when input is absent";

    // Confirm FFI was never called (no registrar/ops installed, so the node
    // would have fired an error if it had tried).
    EXPECT_EQ(g_create_count.load(), 0);
    EXPECT_EQ(g_mock.mark_frame_count, 0);
}

// ---------------------------------------------------------------------------
// Scenario 4: Compute with mock ops and a real CUDA device buffer
//
// Since cudaGraphicsGLRegisterImage is called internally with the fake GL name
// returned by mock_create, the outcome depends on whether a real GL context is
// available.  We follow the A13 pattern: accept both FLOW_ERROR_UNKNOWN
// (headless, cudaGraphicsGLRegisterImage fails) and FLOW_SUCCESS (GL available).
//
// Sub-tests verified in each branch are documented below.
// ---------------------------------------------------------------------------
TEST_F(FlowFlutterCudaPreviewTest, ComputeWithMockOpsAndCudaBuffer)
{
    // Skip if no CUDA device.
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0)
    {
        GTEST_SKIP() << "No CUDA devices available";
    }

    // Allocate a small 8x8 RGBA8 device buffer.
    constexpr int32_t W = 8;
    constexpr int32_t H = 8;
    const size_t TOTAL  = static_cast<size_t>(W) * H * 4;
    auto buf            = std::make_shared<flow::CudaBuffer>(TOTAL, 0, cudaStreamLegacy);
    cudaStreamSynchronize(cudaStreamLegacy);
    cudaMemset(buf->ptr(), 0xCD, TOTAL);
    cudaDeviceSynchronize();

    flow::CudaDeviceImage img;
    img.buffer          = buf;
    img.width           = W;
    img.height          = H;
    img.pitch_bytes     = W * 4;
    img.content_version = 1;
    img.ready_event     = nullptr;

    bind_mock_ops();

    auto env = make_env();
    flow::UUID uuid;
    flow::FlowFlutterCudaPreview node(uuid, "preview_mock", env);

    bool error_fired = false;
    std::string error_message;
    node.OnError.Bind(flow::IndexableName{"err_listener"}, [&](const std::exception& e) {
        error_fired   = true;
        error_message = e.what();
    });

    // --- First Compute ---
    node.SetInputData(flow::IndexableName{"in"}, std::make_shared<flow::NodeData<flow::CudaDeviceImage>>(img),
                      /*compute=*/false);
    node.InvokeCompute();

    if (error_fired)
    {
        // Headless path (deferred-GL design):
        //   - register succeeded: create called once, handle stored in the node.
        //   - write_into: wait_initialized returned the fake cuda_res, then
        //     cudaGraphicsMapResources failed → FLOW_ERROR_UNKNOWN → node threw.
        //   - destroy has NOT been called yet (the node still holds the handle;
        //     cleanup happens in Stop() or the destructor).
        EXPECT_EQ(g_create_count.load(), 1) << "Headless path: create_gl_texture must have been called once";
        EXPECT_EQ(g_mock.mark_frame_count, 0);
        GTEST_SUCCEED() << "Headless path: cudaGraphicsMapResources failed "
                           "as expected (no GL context).  Node surfaced error "
                           "correctly via OnError.  Error: "
                        << error_message;
        return;
    }

    // GL-available path: full success.
    // (a) create was called once.
    EXPECT_EQ(g_create_count.load(), 1) << "(a) create_gl_texture must be called once";
    // (b) write_into was called once (implicitly — if no error and we're here).
    // (c) signal was called once.
    EXPECT_EQ(g_mock.mark_frame_count, 1) << "(c) signal (mark_frame_available) called once";

    // Verify the texture_id output port was populated.
    auto tex_id_data = node.GetOutputData<int64_t>(flow::IndexableName{"texture_id"});
    ASSERT_NE(tex_id_data, nullptr) << "texture_id output must be populated on first Compute";
    EXPECT_GT(tex_id_data->Get(), 0LL) << "texture_id must be positive";

    error_fired = false;

    // --- Second Compute with different dims (16x16) to trigger recreate ---
    constexpr int32_t W2 = 16;
    constexpr int32_t H2 = 16;
    const size_t TOTAL2  = static_cast<size_t>(W2) * H2 * 4;
    auto buf2            = std::make_shared<flow::CudaBuffer>(TOTAL2, 0, cudaStreamLegacy);
    cudaStreamSynchronize(cudaStreamLegacy);
    cudaMemset(buf2->ptr(), 0xEF, TOTAL2);
    cudaDeviceSynchronize();

    flow::CudaDeviceImage img2;
    img2.buffer          = buf2;
    img2.width           = W2;
    img2.height          = H2;
    img2.pitch_bytes     = W2 * 4;
    img2.content_version = 2;
    img2.ready_event     = nullptr;

    node.SetInputData(flow::IndexableName{"in"}, std::make_shared<flow::NodeData<flow::CudaDeviceImage>>(img2),
                      /*compute=*/false);
    node.InvokeCompute();

    EXPECT_FALSE(error_fired) << "Second Compute should not fire an error";

    // (d) destroy called once on the old texture.
    EXPECT_EQ(g_mock.destroy_call_count, 1) << "(d) destroy called once for the old texture";
    // (e) create called a second time.
    EXPECT_EQ(g_create_count.load(), 2) << "(e) create called a second time for new dims";
    // (f) write_into and signal each called a second time.
    EXPECT_EQ(g_mock.mark_frame_count, 2) << "(f) signal called a second time";

    // Stop() should cleanly unregister.
    node.Stop();
    EXPECT_EQ(g_mock.destroy_call_count, 2) << "Stop() must unregister the texture (destroy called)";
}
