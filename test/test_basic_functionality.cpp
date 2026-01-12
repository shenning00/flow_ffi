#include "flow_ffi.h"

#include <cstring>

#include <gtest/gtest.h>

class BasicFunctionalityTest : public ::testing::Test {
protected:
    void SetUp() override { flow_clear_error(); }

    void TearDown() override { flow_clear_error(); }
};

TEST_F(BasicFunctionalityTest, MemoryManagementHelpers) {
    // Test flow_free_string with nullptr (should not crash)
    flow_free_string(nullptr);

    // Test flow_free_string_array with nullptr (should not crash)
    flow_free_string_array(nullptr, 0);

    // Test flow_free_handle_array with nullptr (should not crash)
    flow_free_handle_array(nullptr);

    // These functions should complete without error
    EXPECT_EQ(flow_get_last_error(), nullptr);
}

// NOTE: This test is now obsolete as all phases have been implemented
// All functions that were previously placeholders are now fully functional
// Keeping this disabled for historical reference
TEST_F(BasicFunctionalityTest, DISABLED_PlaceholderFunctionsReturnNotImplemented) {
    // All environment, module, graph, and node functions are now implemented
    // This test originally checked for "not implemented" errors but is no longer needed
}

TEST_F(BasicFunctionalityTest, ErrorCodesEnumValues) {
    // Verify error code constants have expected values
    EXPECT_EQ(FLOW_SUCCESS, 0);
    EXPECT_EQ(FLOW_ERROR_INVALID_HANDLE, -1);
    EXPECT_EQ(FLOW_ERROR_INVALID_ARGUMENT, -2);
    EXPECT_EQ(FLOW_ERROR_NODE_NOT_FOUND, -3);
    EXPECT_EQ(FLOW_ERROR_PORT_NOT_FOUND, -4);
    EXPECT_EQ(FLOW_ERROR_CONNECTION_FAILED, -5);
    EXPECT_EQ(FLOW_ERROR_MODULE_LOAD_FAILED, -6);
    EXPECT_EQ(FLOW_ERROR_COMPUTATION_FAILED, -7);
    EXPECT_EQ(FLOW_ERROR_OUT_OF_MEMORY, -8);
    EXPECT_EQ(FLOW_ERROR_TYPE_MISMATCH, -9);
    EXPECT_EQ(FLOW_ERROR_NOT_IMPLEMENTED, -10);
    EXPECT_EQ(FLOW_ERROR_UNKNOWN, -999);
}

TEST_F(BasicFunctionalityTest, HandleTypeSizes) {
    // Verify that handle types are pointer-sized
    EXPECT_EQ(sizeof(FlowEnvHandle), sizeof(void*));
    EXPECT_EQ(sizeof(FlowGraphHandle), sizeof(void*));
    EXPECT_EQ(sizeof(FlowNodeHandle), sizeof(void*));
    EXPECT_EQ(sizeof(FlowConnectionHandle), sizeof(void*));
    EXPECT_EQ(sizeof(FlowNodeFactoryHandle), sizeof(void*));
    EXPECT_EQ(sizeof(FlowModuleHandle), sizeof(void*));
    EXPECT_EQ(sizeof(FlowNodeDataHandle), sizeof(void*));
}

TEST_F(BasicFunctionalityTest, CompilationAndLinkage) {
    // This test ensures that all function signatures compile and link correctly
    // We're not testing functionality here, just that the C API is properly exposed

    // Environment functions
    auto env = flow_env_create(1);
    flow_env_destroy(env);
    auto factory = flow_env_get_factory(env);
    auto env_result = flow_env_wait(env);

    // Graph functions
    auto graph = flow_graph_create(env);
    flow_graph_destroy(graph);
    auto node = flow_graph_add_node(graph, "test", "test");
    auto graph_result = flow_graph_remove_node(graph, "test");
    auto retrieved_node = flow_graph_get_node(graph, "test");
    FlowNodeHandle* nodes = nullptr;
    size_t count = 0;
    graph_result = flow_graph_get_nodes(graph, &nodes, &count);
    auto conn = flow_graph_connect_nodes(graph, "src", "out", "dst", "in");
    graph_result = flow_graph_disconnect_nodes(graph, "conn_id");
    graph_result = flow_graph_run(graph);
    graph_result = flow_graph_clear(graph);
    auto json = flow_graph_save_to_json(graph);
    graph_result = flow_graph_load_from_json(graph, "{}");

    // Node functions
    auto id = flow_node_get_id(node);
    auto name = flow_node_get_name(node);
    auto cls = flow_node_get_class(node);
    auto node_result = flow_node_set_name(node, "new_name");
    auto data = flow_node_get_input_data(node, "input");
    node_result = flow_node_set_input_data(node, "input", data);
    auto output_data = flow_node_get_output_data(node, "output");
    node_result = flow_node_clear_input_data(node, "input");
    node_result = flow_node_clear_output_data(node, "output");
    node_result = flow_node_invoke_compute(node);
    auto has_inputs = flow_node_has_connected_inputs(node);
    auto has_outputs = flow_node_has_connected_outputs(node);
    auto validate = flow_node_validate_required_inputs(node);

    // All functions should have been callable (even if they return errors)
    // The fact that this test compiles and runs means the C API is properly defined

    // Clear any errors that accumulated
    flow_clear_error();

    SUCCEED(); // This test passes if it compiles and runs without crashing
}