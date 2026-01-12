#include "flow_ffi.h"

#include <gtest/gtest.h>

// Test the new flow_free_port_metadata function
TEST(PortMetadataFreeTest, FreePortMetadataWithNullPointer) {
    // Should not crash with null pointer
    flow_free_port_metadata(nullptr);
}

TEST(PortMetadataFreeTest, FreePortMetadataWithValidData) {
    // Create a metadata structure on the stack
    FlowPortMetadata metadata;

    // Simulate allocated strings (as done by the API)
    const char* key_str = "test_port";
    const char* json_str = R"({"type":"integer","value":"42"})";

    metadata.key = new char[strlen(key_str) + 1];
    strcpy(const_cast<char*>(metadata.key), key_str);

    metadata.interworking_value_json = new char[strlen(json_str) + 1];
    strcpy(const_cast<char*>(metadata.interworking_value_json), json_str);

    metadata.has_default = true;

    // Free the metadata - should not crash
    flow_free_port_metadata(&metadata);

    // After freeing, the pointers should be null
    EXPECT_EQ(metadata.key, nullptr);
    EXPECT_EQ(metadata.interworking_value_json, nullptr);
}

TEST(PortMetadataFreeTest, FreePortMetadataWithPartialData) {
    FlowPortMetadata metadata;

    // Only allocate one field
    const char* key_str = "test_port";
    metadata.key = new char[strlen(key_str) + 1];
    strcpy(const_cast<char*>(metadata.key), key_str);

    metadata.interworking_value_json = nullptr;
    metadata.has_default = false;

    // Should handle partial data gracefully
    flow_free_port_metadata(&metadata);

    EXPECT_EQ(metadata.key, nullptr);
    EXPECT_EQ(metadata.interworking_value_json, nullptr);
}

TEST(PortMetadataFreeTest, ConsistencyWithArrayFreeFunction) {
    // This test verifies that the single free function and array free function
    // behave consistently

    // Create a single metadata item
    FlowPortMetadata single_metadata;
    const char* key1 = "port1";
    const char* json1 = R"({"type":"string","value":"test"})";

    single_metadata.key = new char[strlen(key1) + 1];
    strcpy(const_cast<char*>(single_metadata.key), key1);

    single_metadata.interworking_value_json = new char[strlen(json1) + 1];
    strcpy(const_cast<char*>(single_metadata.interworking_value_json), json1);

    single_metadata.has_default = true;

    // Create an array with one item
    FlowPortMetadata* metadata_array = new FlowPortMetadata[1];
    const char* key2 = "port2";
    const char* json2 = R"({"type":"integer","value":"100"})";

    metadata_array[0].key = new char[strlen(key2) + 1];
    strcpy(const_cast<char*>(metadata_array[0].key), key2);

    metadata_array[0].interworking_value_json = new char[strlen(json2) + 1];
    strcpy(const_cast<char*>(metadata_array[0].interworking_value_json), json2);

    metadata_array[0].has_default = true;

    // Both should free without crashing
    flow_free_port_metadata(&single_metadata);
    flow_free_port_metadata_array(metadata_array, 1);

    // Verify cleanup
    EXPECT_EQ(single_metadata.key, nullptr);
    EXPECT_EQ(single_metadata.interworking_value_json, nullptr);
}
