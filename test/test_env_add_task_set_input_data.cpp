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
#include "node_wrapper.hpp"
#include "node_data_wrapper.hpp"

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Minimal concrete node used for handle-validation tests only.
// nullptr env is safe as long as InvokeCompute() is never called.
// ---------------------------------------------------------------------------
class NullEnvNode : public flow::Node {
public:
    NullEnvNode()
        : flow::Node(flow::UUID(), "NullEnvNode", "null_env", nullptr) {}

protected:
    void Compute() override {}
};

// ---------------------------------------------------------------------------
// Doubly-useful real node: reads input port "value" (int32_t), writes
// output port "result" (int32_t) = value * 2.  Used for the happy-path tests
// that actually run compute on the worker thread (requires a real Env).
// ---------------------------------------------------------------------------
class DoublerNode : public flow::Node {
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
        int32_t v = 0;
        if (raw) {
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
class EnvAddTaskSetInputDataTest : public ::testing::Test {
protected:
    void SetUp() override {
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

    void TearDown() override {
        if (graph_handle_)   { flow_graph_destroy(graph_handle_);        graph_handle_   = nullptr; }
        if (factory_handle_) { flow_release_handle(factory_handle_);     factory_handle_ = nullptr; }
        if (env_handle_)     { flow_env_destroy(env_handle_);            env_handle_     = nullptr; }

        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();
    }

    // Helper: add a DoublerNode to the test graph, return its FFI handle.
    // Caller does NOT own the returned handle — it is owned by the graph.
    FlowNodeHandle add_doubler(const char* name = "d1") {
        return flow_graph_add_node(graph_handle_, "DoublerNode", name);
    }

    // Helper: create an int32 data handle (caller must flow_release_handle).
    FlowNodeDataHandle make_int_data(int32_t value) {
        return flow_data_create_int(value);
    }

    FlowEnvHandle         env_handle_     = nullptr;
    FlowNodeFactoryHandle factory_handle_ = nullptr;
    FlowGraphHandle       graph_handle_   = nullptr;
};

// ============================================================================
// (1) Handle validation: null / invalid handles → FLOW_ERROR_INVALID_HANDLE
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, NullEnv_ReturnsInvalidHandle) {
    // Create throwaway valid node and data handles.
    auto raw_node = std::make_shared<NullEnvNode>();
    auto* node_raw = flow_ffi::get_or_create_node_handle(raw_node);
    auto node_h = reinterpret_cast<FlowNodeHandle>(node_raw);
    auto data_h = make_int_data(42);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(
        flow_env_add_task_set_input_data(nullptr, node_h, "value", data_h),
        FLOW_ERROR_INVALID_HANDLE);

    flow_release_handle(node_h);
    flow_release_handle(data_h);
}

TEST_F(EnvAddTaskSetInputDataTest, NullNode_ReturnsInvalidHandle) {
    auto data_h = make_int_data(42);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(
        flow_env_add_task_set_input_data(env_handle_, nullptr, "value", data_h),
        FLOW_ERROR_INVALID_HANDLE);

    flow_release_handle(data_h);
}

TEST_F(EnvAddTaskSetInputDataTest, NullData_ReturnsInvalidHandle) {
    auto raw_node = std::make_shared<NullEnvNode>();
    auto* node_raw = flow_ffi::get_or_create_node_handle(raw_node);
    auto node_h = reinterpret_cast<FlowNodeHandle>(node_raw);

    EXPECT_EQ(
        flow_env_add_task_set_input_data(env_handle_, node_h, "value", nullptr),
        FLOW_ERROR_INVALID_HANDLE);

    flow_release_handle(node_h);
}

// ============================================================================
// (2) Port-key validation: null or empty → FLOW_ERROR_INVALID_ARGUMENT
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, NullPortKey_ReturnsInvalidArgument) {
    FlowNodeHandle node_h = add_doubler();
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(7);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(
        flow_env_add_task_set_input_data(env_handle_, node_h, nullptr, data_h),
        FLOW_ERROR_INVALID_ARGUMENT);

    flow_release_handle(data_h);
    // node_h is graph-owned; do not release separately.
}

TEST_F(EnvAddTaskSetInputDataTest, EmptyPortKey_ReturnsInvalidArgument) {
    FlowNodeHandle node_h = add_doubler();
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(7);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(
        flow_env_add_task_set_input_data(env_handle_, node_h, "", data_h),
        FLOW_ERROR_INVALID_ARGUMENT);

    flow_release_handle(data_h);
}

// ============================================================================
// (3) Happy path: post a value, wait, read back via flow_node_get_input_data
// ============================================================================

TEST_F(EnvAddTaskSetInputDataTest, HappyPath_PostAndWait_InputDataMatches) {
    FlowNodeHandle node_h = add_doubler();
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(21);
    ASSERT_NE(data_h, nullptr);

    // Post the task — should return immediately.
    EXPECT_EQ(
        flow_env_add_task_set_input_data(env_handle_, node_h, "value", data_h),
        FLOW_SUCCESS);

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

TEST_F(EnvAddTaskSetInputDataTest, HappyPath_ComputeRuns_OutputPortUpdated) {
    FlowNodeHandle node_h = add_doubler("d2");
    ASSERT_NE(node_h, nullptr);

    auto data_h = make_int_data(5);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(
        flow_env_add_task_set_input_data(env_handle_, node_h, "value", data_h),
        FLOW_SUCCESS);

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

TEST_F(EnvAddTaskSetInputDataTest, DataHandleReleasedBeforeLambdaRuns_ComputeStillSucceeds) {
    FlowNodeHandle node_h = add_doubler("d3");
    ASSERT_NE(node_h, nullptr);

    // Value chosen to be distinct from other test values.
    auto data_h = make_int_data(99);
    ASSERT_NE(data_h, nullptr);

    EXPECT_EQ(
        flow_env_add_task_set_input_data(env_handle_, node_h, "value", data_h),
        FLOW_SUCCESS);

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
// (5) Cascade (A→B graph) — deferred.
//
// TODO(P1): Wire a two-node A→B chain via flow_graph_connect_nodes where
// DoublerNode A's "result" port feeds DoublerNode B's "value" port.  Post to
// A's "value", wait, assert B's "result" == input * 4.
//
// This requires the graph layer to propagate SetOutputData events through
// connected ports (EmitUpdate / _propagate_output_update), which works at the
// C++ graph level but the FFI test would need to verify it end-to-end without
// a module providing the connection plumbing.  Deferred to the Dart-side
// widget integration tests (see FLOW_RUN.html §10.11 "Tests").
// ============================================================================
