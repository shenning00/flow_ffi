// test_node_lifecycle.cpp
// P9: tests for flow_node_start / flow_node_stop FFI symbols.
//
// Acceptance criteria (FLOW_RUN.html §10.10):
//   - Default no-op Start()/Stop() return FLOW_SUCCESS.
//   - Invalid handle returns FLOW_ERROR_INVALID_HANDLE.
//   - Double-call is safe (returns SUCCESS twice).
//   - A throwing Start()/Stop() override is caught at the FFI boundary and
//     returns FLOW_ERROR_UNKNOWN with the error manager populated.
//   - A subclass whose Start() sets a flag has the flag set after the call,
//     proving the call actually reached the node.

#include "flow_ffi.h"

#include "handle_manager.hpp"
#include "node_wrapper.hpp"

#include <flow/core/Node.hpp>
#include <flow/core/UUID.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Minimal concrete Node subclass used as the "default" / no-op case.
// Start() and Stop() are inherited as empty virtuals from flow::Node.
// ---------------------------------------------------------------------------
class DefaultNode : public flow::Node
{
  public:
    DefaultNode() : flow::Node(flow::UUID(), "DefaultNode", "default", nullptr) {}

  protected:
    void Compute() override {}
};

// ---------------------------------------------------------------------------
// Flag-flipping node: verifies that Start()/Stop() actually reach the node
// instance via the FFI surface (not just that the FFI symbol returns SUCCESS).
// ---------------------------------------------------------------------------
class FlagNode : public flow::Node
{
  public:
    FlagNode() : flow::Node(flow::UUID(), "FlagNode", "flag", nullptr) {}

    bool started = false;
    bool stopped = false;

    void Start() override { started = true; }
    void Stop() override { stopped = true; }

  protected:
    void Compute() override {}
};

// ---------------------------------------------------------------------------
// Throwing node: Start()/Stop() each throw std::runtime_error.  Verifies the
// FFI try/catch wrapper catches and reports via the ErrorManager rather than
// letting the exception cross the C boundary.
// ---------------------------------------------------------------------------
class ThrowingNode : public flow::Node
{
  public:
    ThrowingNode() : flow::Node(flow::UUID(), "ThrowingNode", "throw", nullptr) {}

    void Start() override { throw std::runtime_error("Start boom"); }
    void Stop() override { throw std::runtime_error("Stop boom"); }

  protected:
    void Compute() override {}
};

// ---------------------------------------------------------------------------
// Fixture: isolated handle registry + cleared error state per test.
// ---------------------------------------------------------------------------
class NodeLifecycleTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();
    }
    void TearDown() override
    {
        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();
    }
};

// ---------------------------------------------------------------------------
// (1) Invalid handle returns FLOW_ERROR_INVALID_HANDLE.
// ---------------------------------------------------------------------------
TEST_F(NodeLifecycleTest, NodeStart_InvalidHandle_ReturnsInvalidHandle)
{
    EXPECT_EQ(flow_node_start(nullptr), FLOW_ERROR_INVALID_HANDLE);
}

TEST_F(NodeLifecycleTest, NodeStop_InvalidHandle_ReturnsInvalidHandle)
{
    EXPECT_EQ(flow_node_stop(nullptr), FLOW_ERROR_INVALID_HANDLE);
}

// ---------------------------------------------------------------------------
// (2) Default no-op Start()/Stop() return FLOW_SUCCESS.
// ---------------------------------------------------------------------------
TEST_F(NodeLifecycleTest, NodeStart_DefaultNoopImpl_ReturnsSuccess)
{
    auto node = std::make_shared<DefaultNode>();
    void* raw = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(raw, nullptr);
    auto handle = reinterpret_cast<FlowNodeHandle>(raw);

    EXPECT_EQ(flow_node_start(handle), FLOW_SUCCESS);

    flow_release_handle(handle);
}

TEST_F(NodeLifecycleTest, NodeStop_DefaultNoopImpl_ReturnsSuccess)
{
    auto node = std::make_shared<DefaultNode>();
    void* raw = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(raw, nullptr);
    auto handle = reinterpret_cast<FlowNodeHandle>(raw);

    EXPECT_EQ(flow_node_stop(handle), FLOW_SUCCESS);

    flow_release_handle(handle);
}

// ---------------------------------------------------------------------------
// (3) Double-call is safe — start; start; stop; stop; all return SUCCESS.
// ---------------------------------------------------------------------------
TEST_F(NodeLifecycleTest, NodeStartStop_DoubleCallIsSafe)
{
    auto node = std::make_shared<DefaultNode>();
    void* raw = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(raw, nullptr);
    auto handle = reinterpret_cast<FlowNodeHandle>(raw);

    EXPECT_EQ(flow_node_start(handle), FLOW_SUCCESS);
    EXPECT_EQ(flow_node_start(handle), FLOW_SUCCESS);
    EXPECT_EQ(flow_node_stop(handle), FLOW_SUCCESS);
    EXPECT_EQ(flow_node_stop(handle), FLOW_SUCCESS);

    flow_release_handle(handle);
}

// ---------------------------------------------------------------------------
// (4) Calls actually reach the node — flag flips on FlagNode.
// ---------------------------------------------------------------------------
TEST_F(NodeLifecycleTest, NodeStart_FlagNode_FlipsStartedFlag)
{
    auto node = std::make_shared<FlagNode>();
    void* raw = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(raw, nullptr);
    auto handle = reinterpret_cast<FlowNodeHandle>(raw);

    EXPECT_FALSE(node->started);
    EXPECT_EQ(flow_node_start(handle), FLOW_SUCCESS);
    EXPECT_TRUE(node->started);

    EXPECT_FALSE(node->stopped);
    EXPECT_EQ(flow_node_stop(handle), FLOW_SUCCESS);
    EXPECT_TRUE(node->stopped);

    flow_release_handle(handle);
}

// ---------------------------------------------------------------------------
// (5) Throwing override → FLOW_ERROR_UNKNOWN, error manager populated, no
//     exception escapes the FFI boundary.
// ---------------------------------------------------------------------------
TEST_F(NodeLifecycleTest, NodeStart_ThrowingImpl_ReturnsUnknownAndSetsError)
{
    auto node = std::make_shared<ThrowingNode>();
    void* raw = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(raw, nullptr);
    auto handle = reinterpret_cast<FlowNodeHandle>(raw);

    EXPECT_EQ(flow_node_start(handle), FLOW_ERROR_UNKNOWN);

    const char* msg = flow_get_last_error();
    ASSERT_NE(msg, nullptr);
    EXPECT_TRUE(std::string(msg).find("Start boom") != std::string::npos)
        << "error message should mention the thrown exception text, got: " << msg;

    flow_release_handle(handle);
}

TEST_F(NodeLifecycleTest, NodeStop_ThrowingImpl_ReturnsUnknownAndSetsError)
{
    auto node = std::make_shared<ThrowingNode>();
    void* raw = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(raw, nullptr);
    auto handle = reinterpret_cast<FlowNodeHandle>(raw);

    EXPECT_EQ(flow_node_stop(handle), FLOW_ERROR_UNKNOWN);

    const char* msg = flow_get_last_error();
    ASSERT_NE(msg, nullptr);
    EXPECT_TRUE(std::string(msg).find("Stop boom") != std::string::npos)
        << "error message should mention the thrown exception text, got: " << msg;

    flow_release_handle(handle);
}
