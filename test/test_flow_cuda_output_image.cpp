// test_flow_cuda_output_image.cpp — P15: FlowCudaOutputImage sink node tests.
//
// Covers: factory registration, port layout, channel property, Start/Stop
// lifecycle, and Compute with mock ops.  Mirrors the style of
// test_flow_flutter_cuda_preview.cpp exactly.

#if FLOW_FFI_HAS_CUDA

#include "flow_ffi.h"
#include "flow_ffi_cuda.h"

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <flow_wire/CudaDeviceImage.hpp>
#include <flow_wire/FlowCudaOutputImage.hpp>
#include <flow_wire/Register.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Mock texture ops — identical sentinel strategy as test_flow_flutter_cuda_preview.cpp
// ---------------------------------------------------------------------------

namespace
{

struct MockState
{
    void* texture_object{nullptr};
    int64_t texture_id{0};
    uint32_t gl_name{0};
    int destroy_call_count{0};
    int mark_frame_count{0};
};

static int s_registrar_sentinel   = 0;
static void* const kMockRegistrar = &s_registrar_sentinel;

static MockState g_mock;
static std::atomic<int> g_create_count{0};
static bool g_create_should_fail{false};

static void reset_mock()
{
    g_mock               = MockState{};
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
    g_mock.texture_id     = 200LL + g_create_count.load();
    g_mock.gl_name        = static_cast<uint32_t>(w * h);
    *out_obj              = g_mock.texture_object;
    *out_id               = g_mock.texture_id;
    *out_name             = g_mock.gl_name;
    return 0;
}

static void mock_destroy(void* /*reg*/, void* /*obj*/, int64_t /*id*/) { ++g_mock.destroy_call_count; }

static void mock_mark_frame(void* /*reg*/, void* /*obj*/) { ++g_mock.mark_frame_count; }

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

static std::shared_ptr<flow::Env> make_env()
{
    auto factory = std::make_shared<flow::NodeFactory>();
    flow::Settings settings;
    settings.MaxThreads = 1;
    return flow::Env::Create(factory, settings);
}

static std::shared_ptr<flow::Env> make_env_with_registry()
{
    auto env = make_env();
    flow_wire::RegisterAllNodes(*env);
    return env;
}

} // namespace

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class FlowCudaOutputImageTest : public ::testing::Test
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
// Scenario 1: Class is registered under "Editor" / "CudaOutputImage"
// ---------------------------------------------------------------------------
TEST_F(FlowCudaOutputImageTest, ClassIsRegisteredInFactory)
{
    auto env     = make_env_with_registry();
    auto factory = env->GetFactory();
    ASSERT_NE(factory, nullptr);

    const auto& cat_map = factory->GetCategories();
    const std::string expected_class{flow::TypeName_v<flow::FlowCudaOutputImage>};

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
    EXPECT_TRUE(found) << "\"" << expected_class << "\" should be in the \"Editor\" category";

    EXPECT_EQ(factory->GetFriendlyName(expected_class), "CudaOutputImage");
}

// ---------------------------------------------------------------------------
// Scenario 2: Port layout — "in" (CudaDeviceImage), "channel" (string),
//             "texture_id" output (int64_t)
// ---------------------------------------------------------------------------
TEST_F(FlowCudaOutputImageTest, PortLayoutIsCorrect)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowCudaOutputImage node(uuid, "cuda_out_test", env);

    const auto& inputs = node.GetInputPorts();
    ASSERT_EQ(inputs.size(), 2u) << "Expected exactly two input ports";

    auto in_it = inputs.find(flow::IndexableName{"in"});
    ASSERT_NE(in_it, inputs.end()) << "Input port \"in\" not found";
    EXPECT_EQ(in_it->second->GetDataType(), flow::TypeName_v<flow::CudaDeviceImage>);

    auto ch_it = inputs.find(flow::IndexableName{"channel"});
    ASSERT_NE(ch_it, inputs.end()) << "Input port \"channel\" not found";
    EXPECT_EQ(ch_it->second->GetDataType(), flow::TypeName_v<std::string>);

    const auto& outputs = node.GetOutputPorts();
    ASSERT_EQ(outputs.size(), 3u) << "Expected three output ports: texture_id, texture_width, texture_height";
    auto out_it = outputs.find(flow::IndexableName{"texture_id"});
    ASSERT_NE(out_it, outputs.end()) << "Output port \"texture_id\" not found";
    EXPECT_EQ(out_it->second->GetDataType(), flow::TypeName_v<int64_t>);

    auto w_it = outputs.find(flow::IndexableName{"texture_width"});
    ASSERT_NE(w_it, outputs.end()) << "Output port \"texture_width\" not found";
    EXPECT_EQ(w_it->second->GetDataType(), flow::TypeName_v<int64_t>);

    auto h_it = outputs.find(flow::IndexableName{"texture_height"});
    ASSERT_NE(h_it, outputs.end()) << "Output port \"texture_height\" not found";
    EXPECT_EQ(h_it->second->GetDataType(), flow::TypeName_v<int64_t>);
}

// ---------------------------------------------------------------------------
// Scenario 3: channel default and settable
// ---------------------------------------------------------------------------
TEST_F(FlowCudaOutputImageTest, ChannelDefaultAndSettable)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowCudaOutputImage node(uuid, "cuda_out_ch", env);

    auto ch_data = node.GetInputData<std::string>(flow::IndexableName{"channel"});
    if (ch_data)
    {
        EXPECT_EQ(ch_data->Get(), "");
    }

    EXPECT_NO_THROW(node.SetInputData(flow::IndexableName{"channel"},
                                      std::make_shared<flow::NodeData<std::string>>(std::string{"main"}),
                                      /*compute=*/false));

    auto ch_after = node.GetInputData<std::string>(flow::IndexableName{"channel"});
    ASSERT_NE(ch_after, nullptr);
    EXPECT_EQ(ch_after->Get(), "main");
}

// ---------------------------------------------------------------------------
// Scenario 4: Start() / Stop() don't crash with no input
// ---------------------------------------------------------------------------
TEST_F(FlowCudaOutputImageTest, StartStopWithNoInputNoCrash)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowCudaOutputImage node(uuid, "cuda_out_lifecycle", env);

    EXPECT_NO_THROW(node.Start());
    EXPECT_NO_THROW(node.Stop());
}

// ---------------------------------------------------------------------------
// Scenario 5: Compute with no input — no-op, no error, no FFI calls
// ---------------------------------------------------------------------------
TEST_F(FlowCudaOutputImageTest, ComputeWithNoInputIsNoOp)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowCudaOutputImage node(uuid, "cuda_out_noop", env);

    bool error_fired = false;
    node.OnError.Bind(flow::IndexableName{"err_listener"}, [&](const std::exception&) { error_fired = true; });

    EXPECT_NO_THROW(node.InvokeCompute());
    EXPECT_FALSE(error_fired);
    EXPECT_EQ(g_create_count.load(), 0);
    EXPECT_EQ(g_mock.mark_frame_count, 0);
}

// ---------------------------------------------------------------------------
// Scenario 6: Compute with mock ops and a real CUDA device buffer.
// Follows the A13 headless/GL-available dual-path pattern.
// ---------------------------------------------------------------------------
TEST_F(FlowCudaOutputImageTest, ComputeWithMockOpsAndCudaBuffer)
{
    int device_count = 0;
    if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count == 0)
    {
        GTEST_SKIP() << "No CUDA devices available";
    }

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
    flow::FlowCudaOutputImage node(uuid, "cuda_out_mock", env);

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
        // Headless path: register succeeded (create called once), then
        // cudaGraphicsMapResources failed — node surfaced error correctly.
        EXPECT_EQ(g_create_count.load(), 1) << "Headless: create_gl_texture must be called once";
        EXPECT_EQ(g_mock.mark_frame_count, 0);
        GTEST_SUCCEED() << "Headless path: cudaGraphicsMapResources failed as expected. "
                           "Error: "
                        << error_message;
        return;
    }

    // GL-available path.
    EXPECT_EQ(g_create_count.load(), 1) << "(a) create called once";
    EXPECT_EQ(g_mock.mark_frame_count, 1) << "(c) signal called once";

    auto tex_id_data = node.GetOutputData<int64_t>(flow::IndexableName{"texture_id"});
    ASSERT_NE(tex_id_data, nullptr) << "texture_id must be populated on first Compute";
    EXPECT_GT(tex_id_data->Get(), 0LL) << "texture_id must be positive";

    auto tex_w_data = node.GetOutputData<int64_t>(flow::IndexableName{"texture_width"});
    ASSERT_NE(tex_w_data, nullptr) << "texture_width must be populated on first Compute";
    EXPECT_EQ(tex_w_data->Get(), static_cast<int64_t>(W));

    auto tex_h_data = node.GetOutputData<int64_t>(flow::IndexableName{"texture_height"});
    ASSERT_NE(tex_h_data, nullptr) << "texture_height must be populated on first Compute";
    EXPECT_EQ(tex_h_data->Get(), static_cast<int64_t>(H));

    error_fired = false;

    // --- Second Compute with different dims to trigger recreate ---
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
    EXPECT_EQ(g_mock.destroy_call_count, 1) << "(d) destroy called once on old texture";
    EXPECT_EQ(g_create_count.load(), 2) << "(e) create called second time for new dims";
    EXPECT_EQ(g_mock.mark_frame_count, 2) << "(f) signal called second time";

    node.Stop();
    EXPECT_EQ(g_mock.destroy_call_count, 2) << "Stop() must unregister the texture";
}

#endif // FLOW_FFI_HAS_CUDA
