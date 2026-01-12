#include "flow_ffi.h"

#include "handle_manager.hpp"
#include <gtest/gtest.h>

class HandleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing handles before each test
        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();
    }

    void TearDown() override {
        // Clean up after each test
        flow_ffi::HandleRegistry::instance().clear();
    }
};

TEST_F(HandleManagerTest, CreateAndValidateHandle) {
    // Create a simple test object
    struct TestObject {
        int value = 42;
    };

    // Create handle
    auto* handle = flow_ffi::create_handle<TestObject>();
    ASSERT_NE(handle, nullptr);

    // Validate handle
    EXPECT_TRUE(flow_is_valid_handle(handle));
    EXPECT_TRUE(flow_ffi::is_valid_handle(handle));

    // Get object from handle
    auto* obj = flow_ffi::get_handle<TestObject>(handle);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);

    // Test reference counting
    EXPECT_EQ(flow_get_ref_count(handle), 1);

    // Retain and check count
    flow_retain_handle(handle);
    EXPECT_EQ(flow_get_ref_count(handle), 2);

    // Release and check count
    flow_release_handle(handle);
    EXPECT_EQ(flow_get_ref_count(handle), 1);

    // Final release should remove handle
    flow_release_handle(handle);
    EXPECT_FALSE(flow_is_valid_handle(handle));
}

TEST_F(HandleManagerTest, InvalidHandleOperations) {
    // Test null pointer
    EXPECT_FALSE(flow_is_valid_handle(nullptr));
    EXPECT_EQ(flow_get_ref_count(nullptr), 0);

    // Test invalid pointer
    void* invalid_ptr = reinterpret_cast<void*>(0xDEADBEEF);
    EXPECT_FALSE(flow_is_valid_handle(invalid_ptr));
    EXPECT_EQ(flow_get_ref_count(invalid_ptr), 0);

    // These operations should not crash
    flow_retain_handle(nullptr);
    flow_release_handle(nullptr);
    flow_retain_handle(invalid_ptr);
    flow_release_handle(invalid_ptr);
}

TEST_F(HandleManagerTest, TypeSafety) {
    struct TypeA {
        int a = 1;
    };
    struct TypeB {
        int b = 2;
    };

    // Create handles of different types
    auto* handle_a = flow_ffi::create_handle<TypeA>();
    auto* handle_b = flow_ffi::create_handle<TypeB>();

    ASSERT_NE(handle_a, nullptr);
    ASSERT_NE(handle_b, nullptr);

    // Get correct types
    auto* obj_a = flow_ffi::get_handle<TypeA>(handle_a);
    auto* obj_b = flow_ffi::get_handle<TypeB>(handle_b);

    ASSERT_NE(obj_a, nullptr);
    ASSERT_NE(obj_b, nullptr);
    EXPECT_EQ(obj_a->a, 1);
    EXPECT_EQ(obj_b->b, 2);

    // Try to get wrong types (should return nullptr)
    auto* wrong_a = flow_ffi::get_handle<TypeB>(handle_a);
    auto* wrong_b = flow_ffi::get_handle<TypeA>(handle_b);

    EXPECT_EQ(wrong_a, nullptr);
    EXPECT_EQ(wrong_b, nullptr);

    // Clean up
    flow_release_handle(handle_a);
    flow_release_handle(handle_b);
}

TEST_F(HandleManagerTest, MultipleReferences) {
    struct TestObject {
        int value = 100;
    };

    auto* handle = flow_ffi::create_handle<TestObject>();
    ASSERT_NE(handle, nullptr);

    // Initial ref count should be 1
    EXPECT_EQ(flow_get_ref_count(handle), 1);

    // Add multiple references
    for (int i = 0; i < 5; ++i) {
        flow_retain_handle(handle);
        EXPECT_EQ(flow_get_ref_count(handle), 2 + i);
    }

    // Release references one by one
    for (int i = 4; i >= 0; --i) {
        EXPECT_TRUE(flow_is_valid_handle(handle));
        flow_release_handle(handle);
        EXPECT_EQ(flow_get_ref_count(handle), 1 + i);
    }

    // Handle should still be valid with one reference
    EXPECT_TRUE(flow_is_valid_handle(handle));
    EXPECT_EQ(flow_get_ref_count(handle), 1);

    // Final release should invalidate handle
    flow_release_handle(handle);
    EXPECT_FALSE(flow_is_valid_handle(handle));
}

TEST_F(HandleManagerTest, HandleRegistry) {
    struct TestObject {
        int id;
    };

    // Registry should be empty initially
    EXPECT_EQ(flow_ffi::HandleRegistry::instance().get_handle_count(), 0);

    // Create multiple handles
    std::vector<void*> handles;
    for (int i = 0; i < 10; ++i) {
        auto* handle = flow_ffi::create_handle<TestObject>(TestObject{i});
        handles.push_back(handle);
    }

    // Registry should have 10 handles
    EXPECT_EQ(flow_ffi::HandleRegistry::instance().get_handle_count(), 10);

    // Verify all handles are valid
    for (auto* handle : handles) {
        EXPECT_TRUE(flow_is_valid_handle(handle));
    }

    // Release all handles
    for (auto* handle : handles) {
        flow_release_handle(handle);
    }

    // Registry should be empty again
    EXPECT_EQ(flow_ffi::HandleRegistry::instance().get_handle_count(), 0);
}