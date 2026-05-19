// test_node_identity_registry.cpp
// Phase 3 (F2): Node-identity-keyed HandleRegistry tests.
//
// Tests CC1 (symmetric ref accounting), CC2 (thread safety), and CC3
// (dual-map consistency) as defined in the F2 implementation spec.

#include "flow_ffi.h"

#include "handle_manager.hpp"
#include "node_wrapper.hpp"

#include <flow/core/Node.hpp>
#include <flow/core/UUID.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal concrete Node subclass for testing (no factory, no real Env needed).
// We pass nullptr for env; the Node constructor accepts it without asserting.
// We never call InvokeCompute() so _env is never dereferenced.
// ---------------------------------------------------------------------------
class TestNode : public flow::Node {
public:
    TestNode()
        : flow::Node(flow::UUID(), "TestNode", "test_instance", nullptr) {}

protected:
    void Compute() override {}
};

// ---------------------------------------------------------------------------
// Test fixture: clears the global singleton registry before and after each
// test to ensure isolation between cases.
// ---------------------------------------------------------------------------
class NodeIdentityRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        flow_ffi::HandleRegistry::instance().clear();
    }

    void TearDown() override {
        flow_ffi::HandleRegistry::instance().clear();
    }

    flow::SharedNode make_node() {
        return std::make_shared<TestNode>();
    }
};

// ---------------------------------------------------------------------------
// (a) Two lookups of the same node must return the same void* (identity keyed).
// Covers CC1: initial create starts at refcount 1; second get retains to 2.
// ---------------------------------------------------------------------------
TEST_F(NodeIdentityRegistryTest, SameNodeYieldsSameHandle) {
    auto node = make_node();

    void* h1 = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(h1, nullptr);

    void* h2 = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(h2, nullptr);

    EXPECT_EQ(h1, h2) << "Two lookups of the same node must return the same void*";
    EXPECT_EQ(flow_get_ref_count(h1), 2)
        << "Ref-count must equal number of outstanding hand-outs";

    // Clean up: release once per hand-out.
    flow_release_handle(h1);
    flow_release_handle(h2);
    EXPECT_FALSE(flow_is_valid_handle(h1));
}

// ---------------------------------------------------------------------------
// (b) Refcount increments per hand-out; node erased only after last release.
// Covers CC1: refcount == N outstanding wrappers.
// ---------------------------------------------------------------------------
TEST_F(NodeIdentityRegistryTest, RefcountTracksHandouts) {
    auto node = make_node();

    void* h1 = flow_ffi::get_or_create_node_handle(node);
    EXPECT_EQ(flow_get_ref_count(h1), 1);

    void* h2 = flow_ffi::get_or_create_node_handle(node);
    EXPECT_EQ(flow_get_ref_count(h1), 2);  // same pointer

    void* h3 = flow_ffi::get_or_create_node_handle(node);
    EXPECT_EQ(flow_get_ref_count(h1), 3);

    flow_release_handle(h1);
    EXPECT_TRUE(flow_is_valid_handle(h1))
        << "Handle must survive while other wrappers are outstanding";
    EXPECT_EQ(flow_get_ref_count(h1), 2);

    flow_release_handle(h2);
    EXPECT_TRUE(flow_is_valid_handle(h1));
    EXPECT_EQ(flow_get_ref_count(h1), 1);

    flow_release_handle(h3);
    EXPECT_FALSE(flow_is_valid_handle(h1))
        << "Handle must be erased after last release";
}

// ---------------------------------------------------------------------------
// (c) Release of one wrapper does not unregister while another is outstanding.
// Covers CC1: partial release survival.
// ---------------------------------------------------------------------------
TEST_F(NodeIdentityRegistryTest, PartialReleaseDoesNotInvalidateHandle) {
    auto node = make_node();

    void* h_a = flow_ffi::get_or_create_node_handle(node);
    void* h_b = flow_ffi::get_or_create_node_handle(node);
    ASSERT_EQ(h_a, h_b) << "Both lookups must return the canonical handle";

    flow_release_handle(h_a);  // simulate Dart GC of one wrapper

    // The second wrapper is still live; handle must be valid.
    EXPECT_TRUE(flow_is_valid_handle(h_b));

    // The NodeWrapper's SharedNode still points to the original node.
    auto* wrapper = flow_ffi::get_handle<NodeWrapper>(h_b);
    ASSERT_NE(wrapper, nullptr);
    EXPECT_EQ(wrapper->node.get(), node.get())
        << "Surviving handle must still point to the original node";

    flow_release_handle(h_b);
    EXPECT_FALSE(flow_is_valid_handle(h_b));
}

// ---------------------------------------------------------------------------
// (d) Different nodes must get distinct handles (no aliasing regression).
// ---------------------------------------------------------------------------
TEST_F(NodeIdentityRegistryTest, DifferentNodesGetDistinctHandles) {
    auto node_a = make_node();
    auto node_b = make_node();

    void* h_a = flow_ffi::get_or_create_node_handle(node_a);
    void* h_b = flow_ffi::get_or_create_node_handle(node_b);

    EXPECT_NE(h_a, h_b) << "Different nodes must have distinct handles";
    EXPECT_EQ(flow_ffi::HandleRegistry::instance().get_handle_count(), 2);

    flow_release_handle(h_a);
    flow_release_handle(h_b);
    EXPECT_EQ(flow_ffi::HandleRegistry::instance().get_handle_count(), 0);
}

// ---------------------------------------------------------------------------
// (e) unregister_handle removes secondary index entry (dual-map consistency,
// CC3): after final release, get_or_create creates a FRESH entry at refcount 1,
// not 2+.
// ---------------------------------------------------------------------------
TEST_F(NodeIdentityRegistryTest, UnregisterClearsSecondaryIndex) {
    auto node = make_node();

    void* h = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(h, nullptr);

    flow_release_handle(h);  // refcount hits 0, both maps cleared.
    EXPECT_FALSE(flow_is_valid_handle(h));

    // Re-creating via get_or_create must produce a NEW entry (no stale secondary hit).
    void* h2 = flow_ffi::get_or_create_node_handle(node);
    ASSERT_NE(h2, nullptr);

    // Refcount must start at 1 (fresh creation), not accumulate from old entry.
    EXPECT_EQ(flow_get_ref_count(h2), 1)
        << "Re-created handle must start at refcount 1, not accumulate from old entry";

    flow_release_handle(h2);
    EXPECT_EQ(flow_ffi::HandleRegistry::instance().get_handle_count(), 0);
}

// ---------------------------------------------------------------------------
// (f) Concurrent add/get/release from multiple threads is race-free.
// Covers CC2: mutex_ serialises all concurrent operations.
// Each thread does kItersPerThread balanced get/release pairs.
// After all threads join, registry must be empty.
// ---------------------------------------------------------------------------
TEST_F(NodeIdentityRegistryTest, ConcurrentHandoutsAreRaceFree) {
    auto node = make_node();
    constexpr int kThreads = 16;
    constexpr int kItersPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&node]() {
            for (int i = 0; i < kItersPerThread; ++i) {
                void* h = flow_ffi::get_or_create_node_handle(node);
                // Brief check and immediate release to maximise interleaving.
                flow_release_handle(h);
            }
        });
    }

    for (auto& th : threads) th.join();

    // After all balanced retains/releases, registry must have zero handles.
    EXPECT_EQ(flow_ffi::HandleRegistry::instance().get_handle_count(), 0)
        << "All handles must be released after balanced retain/release";
}
