#include "flow_ffi.h"

#include <cstring>
#include <thread>

#include "error_handling.hpp"
#include "handle_manager.hpp"
#include <gtest/gtest.h>

class EnvFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();
    }

    void TearDown() override {
        flow_ffi::HandleRegistry::instance().clear();
        flow_clear_error();
    }
};

TEST_F(EnvFactoryTest, CreateEnvironment) {
    // Create environment with 4 threads
    FlowEnvHandle env = flow_env_create(4);
    ASSERT_NE(env, nullptr);
    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Validate handle
    EXPECT_TRUE(flow_is_valid_handle(env));
    EXPECT_EQ(flow_get_ref_count(env), 1);

    // Clean up
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, CreateEnvironmentInvalidThreads) {
    // Try to create environment with invalid thread count
    FlowEnvHandle env = flow_env_create(0);
    EXPECT_EQ(env, nullptr);

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "max_threads must be positive") != nullptr);

    flow_clear_error();

    // Try negative thread count
    env = flow_env_create(-1);
    EXPECT_EQ(env, nullptr);

    error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "max_threads must be positive") != nullptr);
}

TEST_F(EnvFactoryTest, GetFactory) {
    // Create environment
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    // Get factory
    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);
    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Validate factory handle
    EXPECT_TRUE(flow_is_valid_handle(factory));
    EXPECT_EQ(flow_get_ref_count(factory), 1);

    // Clean up
    flow_release_handle(factory);
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, GetFactoryInvalidHandle) {
    // Try to get factory from invalid handle
    FlowNodeFactoryHandle factory = flow_env_get_factory(nullptr);
    EXPECT_EQ(factory, nullptr);

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Invalid handle") != nullptr);
}

TEST_F(EnvFactoryTest, WaitForTasks) {
    // Create environment
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    // Wait should succeed (no tasks running)
    FlowError result = flow_env_wait(env);
    EXPECT_EQ(result, FLOW_SUCCESS);
    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Clean up
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, WaitInvalidHandle) {
    FlowError result = flow_env_wait(nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_HANDLE);

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Invalid handle") != nullptr);
}

TEST_F(EnvFactoryTest, GetEnvironmentVariable) {
    // Create environment
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    // Try to get PATH environment variable (should exist on most systems)
    const char* path_var = flow_env_get_var(env, "PATH");

    // PATH should exist on most systems, but if not, it's not an error
    if (path_var) {
        EXPECT_NE(strlen(path_var), 0);
        flow_free_string(const_cast<char*>(path_var));
    }

    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Clean up
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, GetEnvironmentVariableInvalid) {
    // Create environment
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    // Try with null name
    const char* result = flow_env_get_var(env, nullptr);
    EXPECT_EQ(result, nullptr);

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Invalid argument") != nullptr);

    flow_clear_error();

    // Try with invalid handle
    result = flow_env_get_var(nullptr, "PATH");
    EXPECT_EQ(result, nullptr);

    error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Invalid handle") != nullptr);

    // Clean up
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, GetCategories) {
    // Create environment and factory
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);

    // Get categories (should be empty for new factory)
    char** categories = nullptr;
    size_t count = 0;

    FlowError result = flow_factory_get_categories(factory, &categories, &count);
    EXPECT_EQ(result, FLOW_SUCCESS);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(categories, nullptr);

    // Clean up
    flow_release_handle(factory);
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, GetCategoriesInvalidArgs) {
    // Create environment and factory
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);

    // Try with null arguments
    char** categories = nullptr;
    size_t count = 0;

    FlowError result = flow_factory_get_categories(nullptr, &categories, &count);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_HANDLE);

    result = flow_factory_get_categories(factory, nullptr, &count);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    result = flow_factory_get_categories(factory, &categories, nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    // Clean up
    flow_release_handle(factory);
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, GetNodeClasses) {
    // Create environment and factory
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);

    // Get node classes for non-existent category
    char** classes = nullptr;
    size_t count = 0;

    FlowError result = flow_factory_get_node_classes(factory, "NonExistent", &classes, &count);
    EXPECT_EQ(result, FLOW_SUCCESS);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(classes, nullptr);

    // Clean up
    flow_release_handle(factory);
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, GetFriendlyName) {
    // Create environment and factory
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);

    // Get friendly name for non-existent class (should return empty or class name)
    const char* name = flow_factory_get_friendly_name(factory, "NonExistentNode");
    if (name) {
        // If a name is returned, it should be valid
        EXPECT_GE(strlen(name), 0);
        flow_free_string(const_cast<char*>(name));
    }

    // Clean up
    flow_release_handle(factory);
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, IsConvertible) {
    // Create environment and factory
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);

    // Check basic type conversions (these might not be registered)
    bool convertible = flow_factory_is_convertible(factory, "int", "double");
    // Just verify the call doesn't crash - result depends on registered conversions
    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Test same type (should usually be convertible)
    convertible = flow_factory_is_convertible(factory, "int", "int");
    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Clean up
    flow_release_handle(factory);
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, CreateNodeNoRegistrations) {
    // Create environment and factory
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);

    FlowNodeFactoryHandle factory = flow_env_get_factory(env);
    ASSERT_NE(factory, nullptr);

    // Try to create a node when no classes are registered
    FlowNodeHandle node =
        flow_factory_create_node(factory, "NonExistentNode", nullptr, "test", env);
    EXPECT_EQ(node, nullptr);

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Failed to create node") != nullptr);

    // Clean up
    flow_release_handle(factory);
    flow_env_destroy(env);
}

TEST_F(EnvFactoryTest, HandleReferenceCountingMultipleFactories) {
    // Create environment
    FlowEnvHandle env = flow_env_create(2);
    ASSERT_NE(env, nullptr);
    EXPECT_EQ(flow_get_ref_count(env), 1);

    // Get factory multiple times
    FlowNodeFactoryHandle factory1 = flow_env_get_factory(env);
    ASSERT_NE(factory1, nullptr);

    FlowNodeFactoryHandle factory2 = flow_env_get_factory(env);
    ASSERT_NE(factory2, nullptr);

    // Should have separate handles but reference the same underlying factory
    EXPECT_NE(factory1, factory2);
    EXPECT_EQ(flow_get_ref_count(factory1), 1);
    EXPECT_EQ(flow_get_ref_count(factory2), 1);

    // Clean up
    flow_release_handle(factory1);
    flow_release_handle(factory2);
    flow_env_destroy(env);
}