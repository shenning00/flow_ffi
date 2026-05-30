// test_flutter_texture_sink.cpp — P12: FlowFlutterTextureSink / GPU texture path tests
//
// Always built — no CUDA gate.  Exercises:
//   1. Mock vtable: register → upload → submit_frame called with correct dims.
//   2. Bytes correctness: submit_frame receives exactly the bytes given to
//      upload_to_texture.
//   3. wait_initialized timeout: register, never call init, upload drops frame
//      after 1 s wait timeout.
//   4. Unregister: destroy called exactly once.
//
// Mock vtable strategy mirrors test_flow_flutter_cuda_preview.cpp's approach:
// a static MockGpuState captures every callback invocation so tests can assert
// call counts and data.

#include "flow_ffi.h"

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <flow_wire/FlowFlutterTextureSink.hpp>
#include <flow_wire/Register.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Mock infrastructure
// ---------------------------------------------------------------------------

namespace
{

struct MockGpuState
{
    // create
    int create_call_count{0};
    bool create_should_fail{false};
    void* texture_object{nullptr};
    int64_t texture_id{0};

    // destroy
    int destroy_call_count{0};

    // mark_frame_available
    int mark_count{0};

    // wait_initialized
    // When wait_should_timeout is true, wait_initialized sleeps 1.1 s and
    // returns -1, simulating the raster thread never calling populate().
    bool wait_should_timeout{false};
    int wait_call_count{0};

    // submit_frame — captures the bytes submitted
    int submit_call_count{0};
    std::vector<uint8_t> submitted_bytes;
    int32_t submitted_width{0};
    int32_t submitted_height{0};
    int32_t submitted_stride{0};
};

// Fake "texture object" to hand back from create.
static int s_registrar_sentinel   = 0;
static void* const kMockRegistrar = &s_registrar_sentinel;
static int s_texture_obj_tag      = 0;

static MockGpuState g_mock;
static std::mutex g_mock_mutex;

static void reset_mock()
{
    std::lock_guard<std::mutex> lk(g_mock_mutex);
    g_mock            = MockGpuState{};
    s_texture_obj_tag = 0;
    flow_ffi_set_gpu_texture_ops(nullptr);
    flow_ffi_set_texture_registrar(nullptr);
}

// Callbacks ---

static int mock_create(void* /*reg*/, int32_t w, int32_t h, void** out_obj, int64_t* out_id)
{
    std::lock_guard<std::mutex> lk(g_mock_mutex);
    ++g_mock.create_call_count;
    if (g_mock.create_should_fail) return -1;
    ++s_texture_obj_tag;
    g_mock.texture_object = reinterpret_cast<void*>(static_cast<uintptr_t>(s_texture_obj_tag));
    g_mock.texture_id     = static_cast<int64_t>(s_texture_obj_tag) + 1000;
    *out_obj              = g_mock.texture_object;
    *out_id               = g_mock.texture_id;
    (void)w;
    (void)h;
    return 0;
}

static void mock_destroy(void* /*reg*/, void* /*obj*/, int64_t /*id*/)
{
    std::lock_guard<std::mutex> lk(g_mock_mutex);
    ++g_mock.destroy_call_count;
}

static void mock_mark_frame(void* /*reg*/, void* /*obj*/)
{
    std::lock_guard<std::mutex> lk(g_mock_mutex);
    ++g_mock.mark_count;
}

static int mock_wait_init(void* /*obj*/)
{
    bool timeout;
    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        timeout = g_mock.wait_should_timeout;
        ++g_mock.wait_call_count;
    }
    if (timeout)
    {
        // Sleep just over 1 second to simulate raster thread never firing.
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        return -1;
    }
    return 0;
}

static int mock_submit_frame(void* /*obj*/, const void* src, int32_t stride, int32_t w, int32_t h)
{
    std::lock_guard<std::mutex> lk(g_mock_mutex);
    ++g_mock.submit_call_count;
    g_mock.submitted_width  = w;
    g_mock.submitted_height = h;
    g_mock.submitted_stride = stride;
    const size_t bytes      = static_cast<size_t>(stride) * static_cast<size_t>(h);
    g_mock.submitted_bytes.assign(static_cast<const uint8_t*>(src), static_cast<const uint8_t*>(src) + bytes);
    return 0;
}

static const FlowGpuTextureOps kMockOps = {
    mock_create, mock_destroy, mock_mark_frame, mock_wait_init, mock_submit_frame,
};

static void install_mock_ops()
{
    flow_ffi_set_texture_registrar(kMockRegistrar);
    flow_ffi_set_gpu_texture_ops(&kMockOps);
}

} // namespace

// ---------------------------------------------------------------------------
// Test: full register→upload→unregister lifecycle
// ---------------------------------------------------------------------------

TEST(FlowFlutterTextureSink, RegisterUploadUnregister)
{
    reset_mock();
    install_mock_ops();

    FlowGpuTextureHandle handle = nullptr;
    int64_t tex_id              = -1;
    FlowError reg_err           = flow_ffi_register_flutter_texture(64, 32, &handle, &tex_id);
    ASSERT_EQ(reg_err, FLOW_SUCCESS);
    ASSERT_NE(handle, nullptr);
    EXPECT_GT(tex_id, 0);

    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        EXPECT_EQ(g_mock.create_call_count, 1);
        EXPECT_EQ(g_mock.destroy_call_count, 0);
    }

    // Upload — mock wait_init returns 0 immediately; submit_frame captures bytes.
    const int32_t w = 64, h = 32;
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4), 0xAB);
    FlowError up_err = flow_ffi_upload_to_texture(handle, pixels.data(), w * 4, w, h);
    EXPECT_EQ(up_err, FLOW_SUCCESS);

    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        EXPECT_EQ(g_mock.wait_call_count, 1);
        EXPECT_EQ(g_mock.submit_call_count, 1);
        EXPECT_EQ(g_mock.submitted_width, w);
        EXPECT_EQ(g_mock.submitted_height, h);
        EXPECT_EQ(g_mock.submitted_stride, w * 4);
    }

    // Unregister.
    FlowError unreg_err = flow_ffi_unregister_flutter_texture(handle);
    EXPECT_EQ(unreg_err, FLOW_SUCCESS);

    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        EXPECT_EQ(g_mock.destroy_call_count, 1);
    }
}

// ---------------------------------------------------------------------------
// Test: bytes passed to submit_frame match what was given to upload_to_texture
// ---------------------------------------------------------------------------

TEST(FlowFlutterTextureSink, BytesRoundtrip)
{
    reset_mock();
    install_mock_ops();

    FlowGpuTextureHandle handle = nullptr;
    int64_t tex_id              = -1;
    ASSERT_EQ(flow_ffi_register_flutter_texture(4, 4, &handle, &tex_id), FLOW_SUCCESS);

    const int32_t w = 4, h = 4;
    // Unique sentinel bytes so we can verify the right data arrived.
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4));
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        pixels[i] = static_cast<uint8_t>(i & 0xFF);
    }

    ASSERT_EQ(flow_ffi_upload_to_texture(handle, pixels.data(), w * 4, w, h), FLOW_SUCCESS);

    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        EXPECT_EQ(g_mock.submit_call_count, 1);
        // submit_frame receives the stride*height byte range;
        // the tight-packed case means it equals pixels exactly.
        ASSERT_EQ(g_mock.submitted_bytes.size(), pixels.size());
        EXPECT_EQ(std::memcmp(g_mock.submitted_bytes.data(), pixels.data(), pixels.size()), 0);
    }

    flow_ffi_unregister_flutter_texture(handle);
}

// ---------------------------------------------------------------------------
// Test: wait_initialized timeout — upload drops frame, returns FLOW_SUCCESS
// ---------------------------------------------------------------------------

TEST(FlowFlutterTextureSink, WaitInitTimeout_DropFrame)
{
    reset_mock();
    install_mock_ops();

    // Arm timeout path.
    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        g_mock.wait_should_timeout = true;
    }

    FlowGpuTextureHandle handle = nullptr;
    int64_t tex_id              = -1;
    ASSERT_EQ(flow_ffi_register_flutter_texture(8, 8, &handle, &tex_id), FLOW_SUCCESS);

    const int32_t w = 8, h = 8;
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4), 0xFF);

    // upload_to_texture should return FLOW_SUCCESS (frame dropped silently).
    FlowError err = flow_ffi_upload_to_texture(handle, pixels.data(), w * 4, w, h);
    EXPECT_EQ(err, FLOW_SUCCESS);

    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        EXPECT_EQ(g_mock.wait_call_count, 1);
        // submit_frame must NOT have been called — frame was dropped.
        EXPECT_EQ(g_mock.submit_call_count, 0);
    }

    flow_ffi_unregister_flutter_texture(handle);
}

// ---------------------------------------------------------------------------
// Helper: build a minimal flow::Env (same pattern as test_flow_flutter_cuda_preview.cpp)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Test: FlowFlutterTextureSink node class is registered
// ---------------------------------------------------------------------------

TEST(FlowFlutterTextureSink, NodeClassRegistered)
{
    auto env     = make_env_with_registry();
    auto factory = env->GetFactory();
    ASSERT_NE(factory, nullptr);

    const auto& cat_map = factory->GetCategories();
    const std::string expected_class{flow::TypeName_v<flow::FlowFlutterTextureSink>};

    bool found = false;
    // The node is registered under "Editor".
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
// Test: port layout — "in" input + "texture_id" output
// ---------------------------------------------------------------------------

TEST(FlowFlutterTextureSink, PortLayout)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowFlutterTextureSink node(uuid, "test_sink", env);

    const auto& inputs = node.GetInputPorts();
    ASSERT_EQ(inputs.size(), 1u);
    auto in_it = inputs.find(flow::IndexableName{"in"});
    ASSERT_NE(in_it, inputs.end()) << "Input port \"in\" not found";

    const auto& outputs = node.GetOutputPorts();
    ASSERT_EQ(outputs.size(), 1u);
    auto out_it = outputs.find(flow::IndexableName{"texture_id"});
    ASSERT_NE(out_it, outputs.end()) << "Output port \"texture_id\" not found";
    EXPECT_EQ(out_it->second->GetDataType(), flow::TypeName_v<int64_t>);
}

// ---------------------------------------------------------------------------
// Test: Compute with no input — no crash, no FFI calls
// ---------------------------------------------------------------------------

TEST(FlowFlutterTextureSink, ComputeNoInput_NoOp)
{
    reset_mock();
    install_mock_ops();

    auto env = make_env();
    flow::UUID uuid;
    flow::FlowFlutterTextureSink node(uuid, "test_sink_noop", env);

    // Drive Compute with no input — should not crash and make no FFI calls.
    bool error_fired = false;
    node.OnError.Bind(flow::IndexableName{"err"}, [&](const std::exception& ex) {
        error_fired = true;
        (void)ex;
    });
    node.InvokeCompute();
    EXPECT_FALSE(error_fired);

    {
        std::lock_guard<std::mutex> lk(g_mock_mutex);
        EXPECT_EQ(g_mock.create_call_count, 0);
        EXPECT_EQ(g_mock.submit_call_count, 0);
    }
}
