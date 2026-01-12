#include "flow_ffi.h"

#include <filesystem>
#include <memory>
#include <string>

#include "error_handling.hpp"
#include <gtest/gtest.h>

namespace fs = std::filesystem;

class ModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        env_ = flow_env_create(2);
        ASSERT_NE(env_, nullptr);

        factory_ = flow_env_get_factory(env_);
        ASSERT_NE(factory_, nullptr);
    }

    void TearDown() override {
        if (factory_) {
            flow_release_handle(factory_);
        }
        if (env_) {
            flow_env_destroy(env_);
        }
    }

    FlowEnvHandle env_ = nullptr;
    FlowNodeFactoryHandle factory_ = nullptr;
};

TEST_F(ModuleTest, ModuleCreationAndDestruction) {
    // Create module
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);
    EXPECT_TRUE(flow_is_valid_handle(module));
    EXPECT_EQ(flow_get_ref_count(module), 1);

    // Check initial state
    EXPECT_FALSE(flow_module_is_loaded(module));
    EXPECT_EQ(flow_module_get_name(module), nullptr);
    EXPECT_EQ(flow_module_get_version(module), nullptr);
    EXPECT_EQ(flow_module_get_author(module), nullptr);
    EXPECT_EQ(flow_module_get_description(module), nullptr);

    // Destroy module
    flow_module_destroy(module);
    EXPECT_FALSE(flow_is_valid_handle(module));
}

TEST_F(ModuleTest, ModuleCreationWithInvalidFactory) {
    // Try to create module with null factory
    auto module = flow_module_create(nullptr);
    EXPECT_EQ(module, nullptr);

    const char* error = flow_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("Invalid factory handle"), std::string::npos);
}

TEST_F(ModuleTest, ModuleLoadWithInvalidHandle) {
    // Test with null handle
    auto result = flow_module_load(nullptr, "/some/path");
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    const char* error = flow_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(ModuleTest, ModuleLoadWithInvalidPath) {
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);

    // Test with null path
    auto result = flow_module_load(module, nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    // Test with empty path
    result = flow_module_load(module, "");
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    // Test with non-existent path
    result = flow_module_load(module, "/nonexistent/path");
    EXPECT_EQ(result, FLOW_ERROR_MODULE_LOAD_FAILED);

    flow_module_destroy(module);
}

TEST_F(ModuleTest, ModuleUnloadWhenNotLoaded) {
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);

    // Should succeed even when not loaded
    auto result = flow_module_unload(module);
    EXPECT_EQ(result, FLOW_SUCCESS);

    flow_module_destroy(module);
}

TEST_F(ModuleTest, ModuleUnloadWithInvalidHandle) {
    auto result = flow_module_unload(nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    const char* error = flow_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(ModuleTest, ModuleRegisterNodesWhenNotLoaded) {
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);

    // Should fail when module is not loaded
    auto result = flow_module_register_nodes(module);
    EXPECT_EQ(result, FLOW_ERROR_MODULE_LOAD_FAILED);

    const char* error = flow_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("not loaded"), std::string::npos);

    flow_module_destroy(module);
}

TEST_F(ModuleTest, ModuleUnregisterNodesWhenNotLoaded) {
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);

    // Should fail when module is not loaded
    auto result = flow_module_unregister_nodes(module);
    EXPECT_EQ(result, FLOW_ERROR_MODULE_LOAD_FAILED);

    const char* error = flow_get_last_error();
    EXPECT_NE(error, nullptr);

    flow_module_destroy(module);
}

TEST_F(ModuleTest, ModuleRegisterWithInvalidHandle) {
    auto result = flow_module_register_nodes(nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    result = flow_module_unregister_nodes(nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);
}

TEST_F(ModuleTest, ModuleIsLoadedWithInvalidHandle) {
    // Should return false for null handle
    EXPECT_FALSE(flow_module_is_loaded(nullptr));
}

TEST_F(ModuleTest, ModuleMetadataWithInvalidHandle) {
    // Should return nullptr for null handle
    EXPECT_EQ(flow_module_get_name(nullptr), nullptr);
    EXPECT_EQ(flow_module_get_version(nullptr), nullptr);
    EXPECT_EQ(flow_module_get_author(nullptr), nullptr);
    EXPECT_EQ(flow_module_get_description(nullptr), nullptr);
}

TEST_F(ModuleTest, ModuleRefCountManagement) {
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);

    // Initial ref count should be 1
    EXPECT_EQ(flow_get_ref_count(module), 1);

    // Retain and check ref count
    flow_retain_handle(module);
    EXPECT_EQ(flow_get_ref_count(module), 2);

    // Release once
    flow_release_handle(module);
    EXPECT_EQ(flow_get_ref_count(module), 1);
    EXPECT_TRUE(flow_is_valid_handle(module));

    // Final destroy
    flow_module_destroy(module);
}

TEST_F(ModuleTest, ModuleHandleValidation) {
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);

    // Should be valid initially
    EXPECT_TRUE(flow_is_valid_handle(module));

    // Should remain valid after operations
    EXPECT_FALSE(flow_module_is_loaded(module));
    EXPECT_TRUE(flow_is_valid_handle(module));

    // Should become invalid after destruction
    flow_module_destroy(module);
    EXPECT_FALSE(flow_is_valid_handle(module));
}

TEST_F(ModuleTest, ErrorHandling) {
    flow_clear_error();

    // Test that error messages are properly set
    auto result = flow_module_load(nullptr, "/some/path");
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);

    const char* error = flow_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0);

    // Clear error
    flow_clear_error();
    error = flow_get_last_error();
    EXPECT_EQ(error, nullptr);
}

TEST_F(ModuleTest, MultipleModules) {
    auto module1 = flow_module_create(factory_);
    auto module2 = flow_module_create(factory_);

    ASSERT_NE(module1, nullptr);
    ASSERT_NE(module2, nullptr);
    EXPECT_NE(module1, module2);

    EXPECT_TRUE(flow_is_valid_handle(module1));
    EXPECT_TRUE(flow_is_valid_handle(module2));

    flow_module_destroy(module1);
    EXPECT_FALSE(flow_is_valid_handle(module1));
    EXPECT_TRUE(flow_is_valid_handle(module2));

    flow_module_destroy(module2);
    EXPECT_FALSE(flow_is_valid_handle(module2));
}

// Integration test for the complete module lifecycle
// Note: This test will fail until actual .fmod modules are available for testing
TEST_F(ModuleTest, DISABLED_CompleteModuleLifecycle) {
    auto module = flow_module_create(factory_);
    ASSERT_NE(module, nullptr);

    // This would be the path to a real module for integration testing
    const char* module_path = "/path/to/test/module";

    // Load module
    auto result = flow_module_load(module, module_path);
    EXPECT_EQ(result, FLOW_SUCCESS);
    EXPECT_TRUE(flow_module_is_loaded(module));

    // Check metadata
    const char* name = flow_module_get_name(module);
    const char* version = flow_module_get_version(module);
    const char* author = flow_module_get_author(module);
    const char* description = flow_module_get_description(module);

    EXPECT_NE(name, nullptr);
    EXPECT_NE(version, nullptr);
    EXPECT_NE(author, nullptr);
    EXPECT_NE(description, nullptr);

    // Register nodes
    result = flow_module_register_nodes(module);
    EXPECT_EQ(result, FLOW_SUCCESS);

    // Unregister nodes
    result = flow_module_unregister_nodes(module);
    EXPECT_EQ(result, FLOW_SUCCESS);

    // Unload module
    result = flow_module_unload(module);
    EXPECT_EQ(result, FLOW_SUCCESS);
    EXPECT_FALSE(flow_module_is_loaded(module));

    flow_module_destroy(module);
}