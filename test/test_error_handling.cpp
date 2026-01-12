#include "flow_ffi.h"

#include <cstring>

#include "error_handling.hpp"
#include <gtest/gtest.h>

class ErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override { flow_clear_error(); }

    void TearDown() override { flow_clear_error(); }
};

TEST_F(ErrorHandlingTest, BasicErrorOperations) {
    // Initially, there should be no error
    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Set an error
    const char* test_message = "Test error message";
    flow_set_error(FLOW_ERROR_INVALID_HANDLE, test_message);

    // Get the error
    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_STREQ(error_msg, test_message);

    // Clear the error
    flow_clear_error();
    EXPECT_EQ(flow_get_last_error(), nullptr);
}

TEST_F(ErrorHandlingTest, ThreadLocalErrors) {
    // Set error in main thread
    flow_set_error(FLOW_ERROR_INVALID_ARGUMENT, "Main thread error");

    std::string main_thread_error;
    std::string other_thread_error;

    // Create another thread and set different error
    std::thread other_thread([&other_thread_error]() {
        // This thread should not see the main thread's error
        EXPECT_EQ(flow_get_last_error(), nullptr);

        // Set error in this thread
        flow_set_error(FLOW_ERROR_NODE_NOT_FOUND, "Other thread error");

        const char* error_msg = flow_get_last_error();
        if (error_msg) {
            other_thread_error = error_msg;
        }
    });

    // Main thread should still have its error
    const char* error_msg = flow_get_last_error();
    if (error_msg) {
        main_thread_error = error_msg;
    }

    other_thread.join();

    EXPECT_EQ(main_thread_error, "Main thread error");
    EXPECT_EQ(other_thread_error, "Other thread error");
}

TEST_F(ErrorHandlingTest, ErrorManager) {
    auto& manager = flow_ffi::ErrorManager::instance();

    // Initially no error
    EXPECT_EQ(manager.get_last_error(), nullptr);
    EXPECT_EQ(manager.get_last_error_code(), FLOW_SUCCESS);

    // Set error through manager
    manager.set_error(FLOW_ERROR_CONNECTION_FAILED, "Connection failed");

    // Check error
    EXPECT_STREQ(manager.get_last_error(), "Connection failed");
    EXPECT_EQ(manager.get_last_error_code(), FLOW_ERROR_CONNECTION_FAILED);

    // Clear error
    manager.clear_error();
    EXPECT_EQ(manager.get_last_error(), nullptr);
    EXPECT_EQ(manager.get_last_error_code(), FLOW_SUCCESS);
}

TEST_F(ErrorHandlingTest, ValidationHelpers) {
    // Test handle validation
    EXPECT_FALSE(flow_ffi::validate_handle(nullptr, "test_handle"));

    // Should have set an error
    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Invalid handle") != nullptr);
    EXPECT_TRUE(strstr(error_msg, "test_handle") != nullptr);

    flow_clear_error();

    // Test string validation
    EXPECT_FALSE(flow_ffi::validate_string(nullptr, "test_string"));

    error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Invalid argument") != nullptr);
    EXPECT_TRUE(strstr(error_msg, "test_string") != nullptr);

    flow_clear_error();

    // Test pointer validation
    EXPECT_FALSE(flow_ffi::validate_pointer(nullptr, "test_pointer"));

    error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_TRUE(strstr(error_msg, "Invalid argument") != nullptr);
    EXPECT_TRUE(strstr(error_msg, "test_pointer") != nullptr);

    flow_clear_error();

    // Test with valid inputs
    int dummy_int = 42;
    const char* dummy_string = "valid";

    EXPECT_TRUE(flow_ffi::validate_string(dummy_string, "valid_string"));
    EXPECT_TRUE(flow_ffi::validate_pointer(&dummy_int, "valid_pointer"));

    // Should be no errors
    EXPECT_EQ(flow_get_last_error(), nullptr);
}

TEST_F(ErrorHandlingTest, ErrorSetterRAII) {
    // Test normal completion without exception
    {
        flow_ffi::ErrorSetter setter;
        // No exception, no error should be set
    }
    EXPECT_EQ(flow_get_last_error(), nullptr);

    // Test explicit error setting
    {
        flow_ffi::ErrorSetter setter;
        setter.set_error(FLOW_ERROR_OUT_OF_MEMORY, "Out of memory");
    }

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_STREQ(error_msg, "Out of memory");

    flow_clear_error();

    // Test clear error
    {
        flow_ffi::ErrorSetter setter;
        setter.set_error(FLOW_ERROR_UNKNOWN, "Test error");
        setter.clear_error();
    }
    EXPECT_EQ(flow_get_last_error(), nullptr);
}

TEST_F(ErrorHandlingTest, NullMessageHandling) {
    // Test setting error with null message
    flow_set_error(FLOW_ERROR_UNKNOWN, nullptr);

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_STREQ(error_msg, "Unknown error");

    flow_clear_error();
}

TEST_F(ErrorHandlingTest, MultipleErrorsOverwrite) {
    // Set first error
    flow_set_error(FLOW_ERROR_INVALID_HANDLE, "First error");

    const char* error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_STREQ(error_msg, "First error");

    // Set second error (should overwrite first)
    flow_set_error(FLOW_ERROR_INVALID_ARGUMENT, "Second error");

    error_msg = flow_get_last_error();
    ASSERT_NE(error_msg, nullptr);
    EXPECT_STREQ(error_msg, "Second error");

    flow_clear_error();
}