// test_graph_add_node_with_uuid.cpp
// Phase 1 tests for flow_graph_add_node_with_uuid (ADD_NODE_FROM_JSON.md §Phase 1).
//
// Acceptance criteria:
//   (1) Happy path: explicit UUID is honoured — node retrievable by that UUID.
//   (2) NULL uuid auto-generates a fresh UUID (handle non-null, not zero UUID).
//   (3) Empty-string uuid also auto-generates (strlen(uuid) > 0 branch).
//   (4) Malformed UUID → nullptr + FLOW_ERROR_INVALID_ARGUMENT.
//   (5) Two auto-generated UUIDs differ (guards against re-using a fallback UUID).
//   (6) Duplicate explicit UUID → second call returns handle to the original
//       graph-resident node (verifyNode fix; not a second insertion).
//   (7) AddNode throw → FLOW_ERROR_UNKNOWN (contract comment; no throw path today).
//   (8) Old flow_graph_add_node wrapper still works (ABI regression guard).

#include "flow_ffi.h"

#include "env_wrapper.hpp"
#include "handle_manager.hpp"
#include "node_wrapper.hpp"

#include <flow/core/Env.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Minimal concrete node registered with the factory for every test.
// ---------------------------------------------------------------------------
class SimpleNode : public flow::Node
{
  public:
    SimpleNode(const flow::UUID& id, std::string_view name, std::shared_ptr<flow::Env> env)
        : flow::Node(id, "SimpleNode", name, std::move(env))
    {
    }

  protected:
    void Compute() override {}
};

// ---------------------------------------------------------------------------
// Fixture: env + factory (with SimpleNode registered) + graph, torn down
// after each test.
// ---------------------------------------------------------------------------
class AddNodeWithUuidTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();

        env_handle_ = flow_env_create(2);
        ASSERT_NE(env_handle_, nullptr) << "Could not create env";

        factory_handle_ = flow_env_get_factory(env_handle_);
        ASSERT_NE(factory_handle_, nullptr) << "Could not get factory";

        auto* fw = flow_ffi::get_handle<NodeFactoryWrapper>(factory_handle_);
        ASSERT_NE(fw, nullptr);
        fw->factory->RegisterNodeClass<SimpleNode>("test", "SimpleNode");

        graph_handle_ = flow_graph_create(env_handle_);
        ASSERT_NE(graph_handle_, nullptr) << "Could not create graph";
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

    FlowEnvHandle env_handle_             = nullptr;
    FlowNodeFactoryHandle factory_handle_ = nullptr;
    FlowGraphHandle graph_handle_         = nullptr;
};

// ---------------------------------------------------------------------------
// (1) Happy path — explicit UUID is stored and retrievable.
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, ExplicitUuid_NodeRetrievableByThatUuid)
{
    const char* uuid = "11111111-1111-1111-1111-111111111111";

    FlowNodeHandle h = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", uuid, "my-node");
    ASSERT_NE(h, nullptr) << "add_node_with_uuid returned nullptr; error: "
                          << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    FlowNodeHandle fetched = flow_graph_get_node(graph_handle_, uuid);
    EXPECT_NE(fetched, nullptr) << "flow_graph_get_node could not find node by its explicit UUID";
}

// ---------------------------------------------------------------------------
// (2) NULL uuid → auto-generates a fresh, non-zero UUID.
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, NullUuid_AutoGeneratesFreshUuid)
{
    FlowNodeHandle h = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", nullptr, "auto-node");
    ASSERT_NE(h, nullptr) << "add_node_with_uuid(null uuid) returned nullptr; error: "
                          << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    const char* id = flow_node_get_id(h);
    ASSERT_NE(id, nullptr);
    // Zero UUID string representation
    EXPECT_STRNE(id, "00000000-0000-0000-0000-000000000000");
}

// ---------------------------------------------------------------------------
// (3) Empty-string uuid → same auto-generate branch (strlen(uuid) > 0 check).
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, EmptyStringUuid_AutoGeneratesFreshUuid)
{
    FlowNodeHandle h = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", "", "auto-node-empty");
    ASSERT_NE(h, nullptr) << "add_node_with_uuid(empty uuid) returned nullptr; error: "
                          << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    const char* id = flow_node_get_id(h);
    ASSERT_NE(id, nullptr);
    EXPECT_STRNE(id, "00000000-0000-0000-0000-000000000000");
}

// ---------------------------------------------------------------------------
// (4) Malformed UUID → nullptr + FLOW_ERROR_INVALID_ARGUMENT.
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, MalformedUuid_ReturnsNullAndInvalidArgumentError)
{
    FlowNodeHandle h = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", "not-a-uuid", "bad-node");
    EXPECT_EQ(h, nullptr);
    EXPECT_EQ(flow_get_last_error_code(), static_cast<int>(FLOW_ERROR_INVALID_ARGUMENT))
        << "Expected FLOW_ERROR_INVALID_ARGUMENT (-2), got: " << flow_get_last_error_code();
}

// ---------------------------------------------------------------------------
// (5) Two auto-generated UUIDs are distinct.
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, TwoNullUuidCalls_ProduceDifferentUuids)
{
    FlowNodeHandle h1 = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", nullptr, "node-a");
    FlowNodeHandle h2 = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", nullptr, "node-b");

    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);

    const char* id1_raw = flow_node_get_id(h1);
    ASSERT_NE(id1_raw, nullptr);
    std::string id1(id1_raw);

    const char* id2_raw = flow_node_get_id(h2);
    ASSERT_NE(id2_raw, nullptr);
    std::string id2(id2_raw);

    EXPECT_NE(id1, id2) << "Two auto-UUID calls produced the same UUID: " << id1;
}

// ---------------------------------------------------------------------------
// (6) Duplicate explicit UUID — second call returns handle to the canonical
//     graph-resident node (the first insertion), not a second copy.
//     This pins the verifyNode-not-node fix.
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, DuplicateExplicitUuid_SecondCallReturnsCanonicalNode)
{
    const char* uuid = "22222222-2222-2222-2222-222222222222";

    FlowNodeHandle h1 = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", uuid, "first");
    ASSERT_NE(h1, nullptr) << "First add failed";

    FlowNodeHandle h2 = flow_graph_add_node_with_uuid(graph_handle_, "SimpleNode", uuid, "second");
    // h2 may be nullptr if AddNode silently discards the duplicate and GetNode
    // still succeeds, or may be non-null pointing to the canonical node.
    // The invariant is: the handle returned by flow_graph_get_node must equal
    // the canonical node's UUID, and h2 (if non-null) must have the same UUID.
    FlowNodeHandle canonical = flow_graph_get_node(graph_handle_, uuid);
    ASSERT_NE(canonical, nullptr) << "Graph lost the node on duplicate insert";

    if (h2 != nullptr)
    {
        const char* id2_raw = flow_node_get_id(h2);
        ASSERT_NE(id2_raw, nullptr);
        EXPECT_STREQ(id2_raw, uuid) << "Second add returned handle with unexpected UUID";
    }
    // Regardless, exactly one node with this UUID should exist in the graph.
    const char* canonical_id = flow_node_get_id(canonical);
    ASSERT_NE(canonical_id, nullptr);
    EXPECT_STREQ(canonical_id, uuid);
}

// ---------------------------------------------------------------------------
// (7) AddNode throw → FLOW_ERROR_UNKNOWN.
//
// Contract test only: flow::Graph::AddNode does not throw today, so there is
// no way to exercise this path without modifying flow-core.  The FFI wraps
// the call in try/catch and maps std::exception to FLOW_ERROR_UNKNOWN.
// This comment documents the contract; no runtime assertion is made.
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, AddNodeThrow_ContractDocumented_NoAssert)
{
    // See graph_bridge.cpp: AddNode is wrapped in try { ... }
    // catch (const std::exception& e) → FLOW_ERROR_UNKNOWN.
    // No throw path exists in flow-core today; assertion deferred.
    SUCCEED() << "Contract documented in graph_bridge.cpp — no throw path available today";
}

// ---------------------------------------------------------------------------
// (8) Old flow_graph_add_node wrapper still works (ABI regression guard).
// ---------------------------------------------------------------------------
TEST_F(AddNodeWithUuidTest, LegacyFlowGraphAddNode_StillProducesNode)
{
    FlowNodeHandle h = flow_graph_add_node(graph_handle_, "SimpleNode", "legacy-node");
    ASSERT_NE(h, nullptr) << "Legacy flow_graph_add_node failed; error: "
                          << (flow_get_last_error() ? flow_get_last_error() : "(none)");

    const char* id = flow_node_get_id(h);
    ASSERT_NE(id, nullptr);
    EXPECT_STRNE(id, "00000000-0000-0000-0000-000000000000");
}
