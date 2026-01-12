#include "flow_ffi.h"

#include <cstring>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class PortMetadataTest : public ::testing::Test {
protected:
    FlowEnvHandle env = nullptr;
    FlowGraphHandle graph = nullptr;
    FlowNodeHandle node = nullptr;

    void SetUp() override {
        flow_clear_error();

        // Create environment
        env = flow_env_create(4);
        ASSERT_NE(env, nullptr) << "Failed to create environment: " << flow_get_last_error();

        // Get factory
        FlowNodeFactoryHandle factory = flow_env_get_factory(env);
        ASSERT_NE(factory, nullptr);

        // Load a module that provides test nodes
        FlowModuleHandle module = flow_module_create(factory);
        ASSERT_NE(module, nullptr);

        // Try to load a test module
        const char* test_module_path = std::getenv("TEST_MODULE_PATH");
        if (test_module_path != nullptr) {
            FlowError load_result = flow_module_load(module, test_module_path);
            if (load_result == FLOW_SUCCESS) {
                flow_module_register_nodes(module);
            }
        }

        // Create graph
        graph = flow_graph_create(env);
        ASSERT_NE(graph, nullptr);

        // Add a test node
        // Note: This will depend on having a module loaded with test nodes
        node = flow_graph_add_node(graph, "test.metadata_node", "test_node");

        flow_clear_error();
    }

    void TearDown() override {
        if (graph) {
            flow_graph_destroy(graph);
        }
        if (env) {
            flow_env_destroy(env);
        }
        flow_clear_error();
    }
};

TEST_F(PortMetadataTest, GetPortMetadataInvalidHandle) {
    FlowPortMetadata metadata;

    FlowError result = flow_node_get_port_metadata(nullptr, "test_port", &metadata);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_HANDLE);

    const char* error = flow_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(PortMetadataTest, GetPortMetadataInvalidPortKey) {
    if (!node) {
        GTEST_SKIP() << "No test node available";
    }

    FlowPortMetadata metadata;

    FlowError result = flow_node_get_port_metadata(node, nullptr, &metadata);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);
}

TEST_F(PortMetadataTest, GetPortMetadataInvalidMetadataPointer) {
    if (!node) {
        GTEST_SKIP() << "No test node available";
    }

    FlowError result = flow_node_get_port_metadata(node, "test_port", nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);
}

TEST_F(PortMetadataTest, GetPortMetadataPortNotFound) {
    if (!node) {
        GTEST_SKIP() << "No test node available";
    }

    FlowPortMetadata metadata;

    FlowError result = flow_node_get_port_metadata(node, "nonexistent_port", &metadata);
    EXPECT_EQ(result, FLOW_ERROR_PORT_NOT_FOUND);
}

TEST_F(PortMetadataTest, GetInputPortsMetadataInvalidHandle) {
    FlowPortMetadata* metadata_array = nullptr;
    size_t count = 0;

    FlowError result = flow_node_get_input_ports_metadata(nullptr, &metadata_array, &count);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_HANDLE);
}

TEST_F(PortMetadataTest, GetInputPortsMetadataInvalidArrayPointer) {
    if (!node) {
        GTEST_SKIP() << "No test node available";
    }

    size_t count = 0;

    FlowError result = flow_node_get_input_ports_metadata(node, nullptr, &count);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);
}

TEST_F(PortMetadataTest, GetInputPortsMetadataInvalidCountPointer) {
    if (!node) {
        GTEST_SKIP() << "No test node available";
    }

    FlowPortMetadata* metadata_array = nullptr;

    FlowError result = flow_node_get_input_ports_metadata(node, &metadata_array, nullptr);
    EXPECT_EQ(result, FLOW_ERROR_INVALID_ARGUMENT);
}

TEST_F(PortMetadataTest, GetInputPortsMetadataNoInputPorts) {
    if (!node) {
        GTEST_SKIP() << "No test node available";
    }

    FlowPortMetadata* metadata_array = nullptr;
    size_t count = 0;

    FlowError result = flow_node_get_input_ports_metadata(node, &metadata_array, &count);

    // Even if there are no input ports, the call should succeed
    if (result == FLOW_SUCCESS && count == 0) {
        EXPECT_EQ(metadata_array, nullptr);
    }
}

TEST_F(PortMetadataTest, FreePortMetadataArrayNullPointer) {
    // Should not crash with null pointer
    flow_free_port_metadata_array(nullptr, 0);

    // No assertions needed - test passes if it doesn't crash
}

TEST_F(PortMetadataTest, GetInputPortsMetadataAndFree) {
    if (!node) {
        GTEST_SKIP() << "No test node available";
    }

    FlowPortMetadata* metadata_array = nullptr;
    size_t count = 0;

    FlowError result = flow_node_get_input_ports_metadata(node, &metadata_array, &count);
    EXPECT_EQ(result, FLOW_SUCCESS);

    if (count > 0) {
        ASSERT_NE(metadata_array, nullptr);

        // Verify metadata structure
        for (size_t i = 0; i < count; i++) {
            EXPECT_NE(metadata_array[i].key, nullptr);
            EXPECT_NE(metadata_array[i].interworking_value_json, nullptr);

            // Parse JSON to verify format
            try {
                json j = json::parse(metadata_array[i].interworking_value_json);

                // Verify JSON structure
                EXPECT_TRUE(j.contains("type"));
                EXPECT_TRUE(j["type"].is_string());

                std::string type = j["type"];
                EXPECT_TRUE(type == "integer" || type == "float" || type == "boolean" ||
                            type == "string" || type == "none");

                // If type is not "none" and has_default is true, should have value
                if (type != "none" && metadata_array[i].has_default) {
                    EXPECT_TRUE(j.contains("value"));
                }

            } catch (const json::exception& e) {
                FAIL() << "Failed to parse JSON: " << e.what();
            }
        }

        // Free the metadata array
        flow_free_port_metadata_array(metadata_array, count);
    }
}

TEST_F(PortMetadataTest, JsonFormatValidation) {
    // Test that the JSON format matches the specification
    // This is more of a documentation test

    // Expected formats:
    // {"type":"string","value":"/home/user/file.png"}
    // {"type":"integer","value":"640"}
    // {"type":"float","value":"2.5"}
    // {"type":"boolean","value":"true"}
    // {"type":"none"}

    // Verify we can parse all expected formats
    std::vector<std::string> test_jsons = {
        R"({"type":"string","value":"test"})", R"({"type":"integer","value":"42"})",
        R"({"type":"float","value":"3.14"})", R"({"type":"boolean","value":"true"})",
        R"({"type":"none"})"};

    for (const auto& json_str : test_jsons) {
        try {
            json j = json::parse(json_str);
            EXPECT_TRUE(j.contains("type"));
            EXPECT_TRUE(j["type"].is_string());
        } catch (const json::exception& e) {
            FAIL() << "Failed to parse test JSON: " << json_str << " - " << e.what();
        }
    }
}

// Integration test with actual node creation
TEST_F(PortMetadataTest, IntegrationTestWithRealNode) {
    if (!node) {
        GTEST_SKIP() << "No test node available - requires TEST_MODULE_PATH environment variable";
    }

    // Get input port keys first
    char** port_keys = nullptr;
    size_t port_count = 0;

    FlowError result = flow_node_get_input_port_keys(node, &port_keys, &port_count);
    if (result != FLOW_SUCCESS || port_count == 0) {
        GTEST_SKIP() << "Node has no input ports";
    }

    ASSERT_NE(port_keys, nullptr);

    // Get metadata for first port
    FlowPortMetadata metadata;
    result = flow_node_get_port_metadata(node, port_keys[0], &metadata);
    EXPECT_EQ(result, FLOW_SUCCESS);

    if (result == FLOW_SUCCESS) {
        EXPECT_NE(metadata.key, nullptr);
        EXPECT_NE(metadata.interworking_value_json, nullptr);
        EXPECT_STREQ(metadata.key, port_keys[0]);

        // Parse and verify JSON
        try {
            json j = json::parse(metadata.interworking_value_json);
            EXPECT_TRUE(j.contains("type"));
        } catch (const json::exception& e) {
            FAIL() << "Invalid JSON format: " << e.what();
        }

        // Free metadata using the proper API function
        flow_free_port_metadata(&metadata);
    }

    // Free port keys
    flow_free_string_array(port_keys, port_count);
}
