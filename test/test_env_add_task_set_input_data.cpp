// test_env_add_task_set_input_data.cpp
// P1 tests for flow_env_add_task_set_input_data (FLOW_RUN.html §10.11).
//
// Acceptance criteria:
//   (1) Null/invalid env, node, data handles → FLOW_ERROR_INVALID_HANDLE.
//   (2) Null or empty port_key → FLOW_ERROR_INVALID_ARGUMENT.
//   (3) Happy path: post a value, wait, read back via flow_node_get_input_data.
//   (4) Lifetime: data FFI handle released before lambda runs; compute still
//       sees the original value because the shared_ptr in the lambda keeps the
//       NodeData alive.
//   (5) Cascade (A→B graph): deferred — requires wiring SetOutputData through
//       the graph connection layer.  See TODO(P1) below.

#include "flow_ffi.h"

#include "env_wrapper.hpp"
#include "handle_manager.hpp"
#include "node_data_wrapper.hpp"
#include "node_wrapper.hpp"

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Minimal concrete node used for handle-validation tests only.
// nullptr env is safe as long as InvokeCompute() is never called.
// ---------------------------------------------------------------------------
class NullEnvNode : public flow::Node
{
  public:
    NullEnvNode() : flow::Node(flow::UUID(), "NullEnvNode", "null_env", nullptr) {}

  protected:
    void Compute() override {}
};

// ---------------------------------------------------------------------------
// Doubly-useful real node: reads input port "value" (int32_t), writes
// output port "result" (int32_t) = value * 2.  Used for the happy-path tests
// that actually run compute on the worker thread (requires a real Env).
// ---------------------------------------------------------------------------
class DoublerNode : public flow::Node
{
  public:
    DoublerNode(const flow::UUID& id, std::string_view name, std::shared_ptr<flow::Env> env)
        : flow::Node(id, "DoublerNode", name, std::move(env))
    {
        AddInput<int32_t>("value", "Value");
        AddOutput<int32_t>("result", "Result");
    }

  protected:
    void Compute() override
    {
        // The FFI layer creates data as detail::NodeData<int>, not
        // flow::NodeData<int>, so GetInputData<int32_t>() (which does a
        // dynamic_pointer_cast to flow::NodeData<int32_t>) returns null.
        // Access via the base INodeData interface instead.
        const flow::SharedNodeData& raw = GetInputData("value");
        int32_t v                       = 0;
        if (raw)
        {
            auto* typed = dynamic_cast<flow::detail::NodeData<int32_t>*>(raw.get());
            if (typed) v = typed->Get();
        }
        SetOutputData("result", flow::MakeNodeData<int32_t>(v * 2));
    }
};

// ---------------------------------------------------------------------------
// Fixture: clears the global handle registry and error state between tests.
// For tests that need a real Env, the fixture also provides env/factory/graph
// handles, cleaned up in TearDown.
// ---------------------------------------------------------------------------
class EnvAddTaskSetInputDataTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();

        // Real env for happy-path tests.
        env_handle_ = flow_env_create(2);
        ASSERT_NE(env_handle_, nullptr) << "Could not create env for test fixture";

        factory_handle_ = flow_env_get_factory(env_handle_);
        ASSERT_NE(factory_handle_, nullptr) << "Could not get factory from env";

        // Register DoublerNode with the factory so flow_graph_add_node can
        // construct one.
        auto* fw = flow_ffi::get_handle<NodeFactoryWrapper>(factory_handle_);
        ASSERT_NE(fw, nullptr);
        fw->factory->RegisterNodeClass<DoublerNode>("test", "DoublerNode");

        graph_handle_ = flow_graph_create(env_handle_);
        ASSERT_NE(graph_handle_, nullptr) << "Could not create graph for test fixture";
    }

    void TearDown() override
    {
        if (graph_handle_)
        {
            flow_graph_destroy(graph_handle_);
            graph_handle_ = nullptr;
        }
        if (factory_handle_)
        {
            flow_release_handle(factory_handle_);
            factory_handle_ = nullptr;
        }
        if (env_handle_)
        {
            flow_env_destroy(env_handle_);
            env_handle_ = nullptr;
        }

        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();
    }

    // Helper: add a DoublerNode to the test graph, return its FFI handle.
    // Caller does NOT own the returned handle — it is owned by the graph.
    FlowNodeHandle add_doubler(const char* name = "d1")
    {
        return flow_graph_add_node(graph_handle_, "DoublerNode", name);
    }

    // Helper: create an int32 data handle (caller must flow_release_handle).
    FlowNodeDataHandle make_int_data(int32_t value) { return flow_data_create_int(value); }

    FlowEnvHandle env_handle_             = nullptr;
    FlowNodeFactoryHandle factory_handle_ = nullptr;
    FlowGraphHandle graph_handle_         = nullptr;
};

// ============================================================================
// (1) Handle validation: null / invalid handles → FLOW_ERROR_INVALID_HANDLE
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, NullEnv_ReturnsInvalidHandle)
{
    // Create throwaway valid node and data handles.
    auto raw_node  = std::make_shared<NullEnvNode>();
    auto* node_raw = flow_ffi::get_or_create_node_handle(raw_node);
    auto node_h    = reinterpret_cast<FlowNodeHandle>(node_raw);
    auto data_h    = make_int_data(42);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(flow_env_add_task_set_input_data(nullptr, node_h, "value", data_h), FLOW_ERROR_INVALID_HANDLE);

    flow_release_handle(node_h);
    flow_release_handle(data_h);
}

TEST_F(EnvAddTaskSetInputDataTest, NullNode_ReturnsInvalidHandle)
{
    auto data_h = make_int_data(42);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, nullptr, "value", data_h), FLOW_ERROR_INVALID_HANDLE);

    flow_release_handle(data_h);
}

TEST_F(EnvAddTaskSetInputDataTest, NullData_ReturnsInvalidHandle)
{
    auto raw_node  = std::make_shared<NullEnvNode>();
    auto* node_raw = flow_ffi::get_or_create_node_handle(raw_node);
    auto node_h    = reinterpret_cast<FlowNodeHandle>(node_raw);

    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_h, "value", nullptr), FLOW_ERROR_INVALID_HANDLE);

    flow_release_handle(node_h);
}

// ============================================================================
// (2) Port-key validation: null or empty → FLOW_ERROR_INVALID_ARGUMENT
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, NullPortKey_ReturnsInvalidArgument)
{
    FlowNodeHandle node_h = add_doubler();
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(7);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_h, nullptr, data_h), FLOW_ERROR_INVALID_ARGUMENT);

    flow_release_handle(data_h);
    // node_h is graph-owned; do not release separately.
}

TEST_F(EnvAddTaskSetInputDataTest, EmptyPortKey_ReturnsInvalidArgument)
{
    FlowNodeHandle node_h = add_doubler();
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(7);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_h, "", data_h), FLOW_ERROR_INVALID_ARGUMENT);

    flow_release_handle(data_h);
}

// ============================================================================
// (3) Happy path: post a value, wait, read back via flow_node_get_input_data
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, HappyPath_PostAndWait_InputDataMatches)
{
    FlowNodeHandle node_h = add_doubler();
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(21);
    ASSERT_NE(data_h, nullptr);

    // Post the task — should return immediately.
    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_h, "value", data_h), FLOW_SUCCESS);

    // Caller may release the FFI data handle right away (spec lifetime contract).
    flow_release_handle(data_h);
    data_h = nullptr;

    // Block until the worker thread completes the task.
    EXPECT_EQ(flow_env_wait(env_handle_), FLOW_SUCCESS);

    // Read back the input port — must match what was posted.
    FlowNodeDataHandle read_h = flow_node_get_input_data(node_h, "value");
    ASSERT_NE(read_h, nullptr) << "Expected input port 'value' to have data after worker ran";

    int32_t got = 0;
    ASSERT_EQ(flow_data_get_int(read_h, &got), FLOW_SUCCESS);
    EXPECT_EQ(got, 21) << "Worker must have written the posted value to the input port";

    flow_release_handle(read_h);
}

TEST_F(EnvAddTaskSetInputDataTest, HappyPath_ComputeRuns_OutputPortUpdated)
{
    FlowNodeHandle node_h = add_doubler("d2");
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(5);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_h, "value", data_h), FLOW_SUCCESS);

    flow_release_handle(data_h);
    data_h = nullptr;

    EXPECT_EQ(flow_env_wait(env_handle_), FLOW_SUCCESS);

    // DoublerNode::Compute() writes value*2 to "result".
    FlowNodeDataHandle out_h = flow_node_get_output_data(node_h, "result");
    ASSERT_NE(out_h, nullptr) << "Expected output port 'result' to have data after compute";

    int32_t result = 0;
    ASSERT_EQ(flow_data_get_int(out_h, &result), FLOW_SUCCESS);
    EXPECT_EQ(result, 10) << "DoublerNode must have doubled the input (5 * 2 == 10)";

    flow_release_handle(out_h);
}

// ============================================================================
// (4) Lifetime: data handle released *before* the lambda runs.
//     shared_ptr in the lambda keeps the NodeData alive; compute must still
//     see the original value.
//
// We post the task, then immediately release the FFI data handle, then wait.
// This is strictly the same as (3) above in terms of sequencing — the spec
// guarantees this is safe — but the test makes the intent explicit and is
// labelled to match the spec's "data-handle-released-but-compute-still-runs"
// acceptance criterion.
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, DataHandleReleasedBeforeLambdaRuns_ComputeStillSucceeds)
{
    FlowNodeHandle node_h = add_doubler("d3");
    ASSERT_NE(node_h, nullptr);

    // Value chosen to be distinct from other test values.
    auto data_h = make_int_data(99);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_h, "value", data_h), FLOW_SUCCESS);

    // Release the FFI handle immediately after posting — before the worker has
    // had any chance to run. The shared_ptr captured in the lambda must keep
    // the underlying NodeData alive independently.
    flow_release_handle(data_h);
    data_h = nullptr;

    EXPECT_EQ(flow_env_wait(env_handle_), FLOW_SUCCESS);

    FlowNodeDataHandle in_h = flow_node_get_input_data(node_h, "value");
    ASSERT_NE(in_h, nullptr) << "Input port must have data even when FFI handle was released first";

    int32_t got = 0;
    ASSERT_EQ(flow_data_get_int(in_h, &got), FLOW_SUCCESS);
    EXPECT_EQ(got, 99) << "Shared_ptr in the lambda must have kept the NodeData alive";

    flow_release_handle(in_h);

    // Compute should also have run: output = 99 * 2 = 198.
    FlowNodeDataHandle out_h = flow_node_get_output_data(node_h, "result");
    ASSERT_NE(out_h, nullptr);

    int32_t result = 0;
    ASSERT_EQ(flow_data_get_int(out_h, &result), FLOW_SUCCESS);
    EXPECT_EQ(result, 198);

    flow_release_handle(out_h);
}

// ============================================================================
// (5) Cascade (A→B graph) — drives the full propagation chain end-to-end:
//
//     post.setInputData(A.value, 5)
//       └→ AddTask(SetInputData(A.value, 5, compute=true))
//          └→ A.InvokeCompute() → A.Compute() doubles 5 → SetOutputData(A.result, 10)
//             └→ EmitUpdate(A.result, 10)
//                └→ _propagate_output_update → Graph::PropagateConnectionsData
//                   └→ for each connection from A.result: AddTask(...)
//                      └→ SetInputData(B.value, 10) (compute=true default)
//                         └→ B.InvokeCompute() → B.Compute() doubles 10
//                            → SetOutputData(B.result, 20)
//
// Reproduces the user-visible setup (Dart bridge ↔ test_module nodes) at the
// C++ level. If this passes but the Flutter app still doesn't cascade, the
// issue is on the Dart/Flutter side (test_module node config, bridge wiring,
// module loader). If it fails, the cascade is broken at the C++ level and the
// failure here narrows the bug.
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, Cascade_AtoB_DoublesTwice)
{
    // Add A and B (both DoublerNodes), then connect A.result → B.value.
    FlowNodeHandle node_a = add_doubler("d_a");
    FlowNodeHandle node_b = add_doubler("d_b");
    ASSERT_NE(node_a, nullptr);
    ASSERT_NE(node_b, nullptr);

    const char* a_id = flow_node_get_id(node_a);
    const char* b_id = flow_node_get_id(node_b);
    ASSERT_NE(a_id, nullptr);
    ASSERT_NE(b_id, nullptr);

    // Take owned copies — flow_node_get_id may return a pointer into a
    // single rotating buffer that gets overwritten by the second call.
    std::string a_id_str = a_id;
    std::string b_id_str = b_id;

    FlowConnectionHandle conn =
        flow_graph_connect_nodes(graph_handle_, a_id_str.c_str(), "result", b_id_str.c_str(), "value");
    ASSERT_NE(conn, nullptr) << "flow_graph_connect_nodes failed — last_error: "
                             << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    // Post 5 to A.value via the new symbol.  No flow_graph_run() — the
    // cascade is the whole point of the test; if it works, we don't need
    // to manually kick anything.
    FlowNodeDataHandle in_h = make_int_data(5);
    ASSERT_NE(in_h, nullptr);
    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_a, "value", in_h), FLOW_SUCCESS);

    flow_release_handle(in_h);
    in_h = nullptr;

    // Drain the whole cascade — the chain may queue multiple tasks before it
    // settles.
    EXPECT_EQ(flow_env_wait(env_handle_), FLOW_SUCCESS);

    // A.result should be 10 (5 * 2).
    FlowNodeDataHandle out_a = flow_node_get_output_data(node_a, "result");
    ASSERT_NE(out_a, nullptr) << "A.result should be populated after the kicked compute";
    int32_t a_val = 0;
    ASSERT_EQ(flow_data_get_int(out_a, &a_val), FLOW_SUCCESS);
    EXPECT_EQ(a_val, 10) << "A should have doubled the input (5 * 2 == 10)";
    flow_release_handle(out_a);

    // B.result should be 20 (10 * 2) IF the cascade propagated.
    // If B is null or 0, the cascade did not reach B — that's the diagnostic.
    FlowNodeDataHandle out_b = flow_node_get_output_data(node_b, "result");
    ASSERT_NE(out_b, nullptr) << "B.result is missing — cascade did NOT reach B. "
                                 "Either PropagateConnectionsData didn't enqueue a task for the "
                                 "A.result → B.value connection, or the queued task threw silently. "
                                 "Check Graph::PropagateConnectionsData and Connections::FindConnections.";
    int32_t b_val = 0;
    ASSERT_EQ(flow_data_get_int(out_b, &b_val), FLOW_SUCCESS);
    EXPECT_EQ(b_val, 20) << "B should have doubled A's output (10 * 2 == 20). "
                            "Got "
                         << b_val << " — the cascade fired but B saw the wrong input.";
    flow_release_handle(out_b);
}

// ============================================================================
// (6) Cascade via module-loaded nodes — repros the exact Flutter scenario.
//
// Loads test_module.fmod (built from flutter_fl_nodes/examples/...) and runs
// PassthroughStringNode → DisplayStringNode through the same FFI path the
// Flutter bridge uses. If this passes but the running app doesn't cascade,
// the bug is squarely on the Dart/Flutter side (event delivery, bridge
// wiring). If it fails, the bug is specific to module-loaded node classes.
//
// The .fmod path is hard-coded to the flutter_fl_nodes fixture — if the
// file is missing (CI, fresh checkout without test_module built), the test
// is GTEST_SKIPped, not failed.
// ============================================================================

namespace
{
constexpr const char* kTestModuleFmodPath =
    "/Users/shenning/Development/flow/flutter_fl_nodes/examples/fl_nodes_example/"
    "test/fixtures/test_module/build/test_module.fmod";
}

TEST_F(EnvAddTaskSetInputDataTest, Cascade_AtoB_ModuleLoaded_StringChain)
{
    // Skip cleanly if the fixture isn't built — the test is a flutter_fl_nodes
    // artifact, not part of flow_ffi's own build.
    {
        std::ifstream probe(kTestModuleFmodPath);
        if (!probe.good())
        {
            GTEST_SKIP() << "test_module.fmod not found at " << kTestModuleFmodPath
                         << " — build the fixture first (cmake --build "
                            "flutter_fl_nodes/examples/.../test_module/build).";
        }
    }

    // Load the module and register its node classes with the factory the
    // fixture already created.
    FlowModuleHandle module = flow_module_create(factory_handle_);
    ASSERT_NE(module, nullptr);

    ASSERT_EQ(flow_module_load(module, kTestModuleFmodPath), FLOW_SUCCESS)
        << "Failed to load test_module.fmod — last_error: "
        << (flow_get_last_error() ? flow_get_last_error() : "(none)");
    ASSERT_TRUE(flow_module_is_loaded(module));

    ASSERT_EQ(flow_module_register_nodes(module), FLOW_SUCCESS)
        << "Failed to register module nodes — last_error: "
        << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    // Create A (PassthroughStringNode) and B (DisplayStringNode). These are
    // the exact two node types the user is chaining in the running app.
    FlowNodeHandle node_a = flow_graph_add_node(graph_handle_, "PassthroughStringNode", "passthrough_a");
    FlowNodeHandle node_b = flow_graph_add_node(graph_handle_, "DisplayStringNode", "display_b");
    ASSERT_NE(node_a, nullptr) << "flow_graph_add_node('PassthroughStringNode') failed — last_error: "
                               << (flow_get_last_error() ? flow_get_last_error() : "(none)");
    ASSERT_NE(node_b, nullptr) << "flow_graph_add_node('DisplayStringNode') failed — last_error: "
                               << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    // Snapshot the UUIDs before any subsequent flow_node_get_id call clobbers
    // the shared return buffer.
    const char* a_id_raw = flow_node_get_id(node_a);
    ASSERT_NE(a_id_raw, nullptr);
    std::string a_id     = a_id_raw;
    const char* b_id_raw = flow_node_get_id(node_b);
    ASSERT_NE(b_id_raw, nullptr);
    std::string b_id = b_id_raw;

    // Connect A.result → B.value (same connection shape the user makes).
    FlowConnectionHandle conn = flow_graph_connect_nodes(graph_handle_, a_id.c_str(), "result", b_id.c_str(), "value");
    ASSERT_NE(conn, nullptr) << "flow_graph_connect_nodes failed — last_error: "
                             << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    // Post "hello" to A.value via the new symbol.
    FlowNodeDataHandle in_h = flow_data_create_string("hello");
    ASSERT_NE(in_h, nullptr);
    EXPECT_EQ(flow_env_add_task_set_input_data(env_handle_, node_a, "value", in_h), FLOW_SUCCESS);

    flow_release_handle(in_h);
    in_h = nullptr;

    // Drain the cascade.
    EXPECT_EQ(flow_env_wait(env_handle_), FLOW_SUCCESS);

    // A.result should be "hello" (PassthroughString just echoes input).
    FlowNodeDataHandle out_a = flow_node_get_output_data(node_a, "result");
    ASSERT_NE(out_a, nullptr) << "A.result missing — PassthroughStringNode.Compute did not fire "
                                 "OR did not call SetOutputData. Most likely SetInputData on the "
                                 "worker thread isn't triggering InvokeCompute for module-loaded "
                                 "nodes.";
    char* a_str = nullptr;
    ASSERT_EQ(flow_data_get_string(out_a, &a_str), FLOW_SUCCESS);
    ASSERT_NE(a_str, nullptr);
    EXPECT_STREQ(a_str, "hello") << "A.result has wrong value";
    std::free(a_str);
    flow_release_handle(out_a);

    // B.result should be "hello" too IF the cascade propagated.
    FlowNodeDataHandle out_b = flow_node_get_output_data(node_b, "result");
    ASSERT_NE(out_b, nullptr) << "*** CASCADE DID NOT REACH B ***\n"
                                 "B.result is null — the A→B propagation did NOT fire. Same "
                                 "symptom as the Flutter app's '(no value)'. The cascade works "
                                 "for inline-defined DoublerNode (see Cascade_AtoB_DoublesTwice) "
                                 "but NOT for module-loaded nodes — the bug is in how module "
                                 "nodes register or in how their EmitUpdate path interacts with "
                                 "Graph::PropagateConnectionsData. Investigate the module-loader "
                                 "AddNode path vs. the in-process Graph::AddNode path.";
    char* b_str = nullptr;
    ASSERT_EQ(flow_data_get_string(out_b, &b_str), FLOW_SUCCESS);
    ASSERT_NE(b_str, nullptr);
    EXPECT_STREQ(b_str, "hello") << "B.result has wrong value — cascade fired but B saw wrong input.";
    std::free(b_str);
    flow_release_handle(out_b);

    // Clean up the loaded module before the fixture tears down the factory.
    flow_module_unregister_nodes(module);
    flow_module_unload(module);
    flow_module_destroy(module);
}
