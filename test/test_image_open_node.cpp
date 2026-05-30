#include "flow_ffi.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include <gtest/gtest.h>

// test_image_open_node.cpp — covers the built-in Editor.ImageOpen node
// added as a P11 follow-up.  The C++ side reads the file's raw bytes and
// emits a flow::Image (kind=Encoded); Skia decodes on the Dart side, so
// the test does not require a real codec — any non-empty file round-trips.

namespace
{

// Helper: write `bytes` to a unique temp file; returns the absolute path.
// Caller is responsible for std::remove() on the returned path.
std::string write_temp_file(const std::vector<uint8_t>& bytes, const std::string& suffix = ".bin")
{
    // mkstemps style isn't portable; tmpnam is deprecated.  Use the
    // POSIX-ish pattern of /tmp + pid + counter, which is more than
    // unique enough for a single-process test run.
    static int counter = 0;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "/tmp/flow_ffi_image_open_test_%d_%d%s", static_cast<int>(::getpid()), counter++,
                  suffix.c_str());
    std::ofstream f(buf, std::ios::binary);
    if (bytes.empty())
    {
        // touch the file so it exists but is empty
        f.close();
        return buf;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return buf;
}

class ImageOpenNodeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        flow_clear_error();
        env_ = flow_env_create(2);
        ASSERT_NE(env_, nullptr);
        factory_ = flow_env_get_factory(env_);
        ASSERT_NE(factory_, nullptr);
        graph_ = flow_graph_create(env_);
        ASSERT_NE(graph_, nullptr);
    }

    void TearDown() override
    {
        if (graph_)
        {
            flow_graph_destroy(graph_);
            graph_ = nullptr;
        }
        if (factory_)
        {
            flow_release_handle(factory_);
            factory_ = nullptr;
        }
        if (env_)
        {
            flow_env_destroy(env_);
            env_ = nullptr;
        }
        flow_clear_error();
    }

    FlowEnvHandle env_             = nullptr;
    FlowNodeFactoryHandle factory_ = nullptr;
    FlowGraphHandle graph_         = nullptr;
};

// (1) Factory registration: Editor category contains an ImageOpen class.
TEST_F(ImageOpenNodeTest, IsRegistered)
{
    char** classes    = nullptr;
    std::size_t count = 0;
    ASSERT_EQ(flow_factory_get_node_classes(factory_, "Editor", &classes, &count), FLOW_SUCCESS);
    ASSERT_GT(count, 0u);

    bool found = false;
    std::string image_open_class;
    for (std::size_t i = 0; i < count; ++i)
    {
        ASSERT_NE(classes[i], nullptr);
        std::string name = classes[i];
        if (name.find("ImageOpenNode") != std::string::npos)
        {
            found            = true;
            image_open_class = name;
        }
    }
    flow_free_string_array(classes, count);

    EXPECT_TRUE(found) << "Expected an Editor.* class containing 'ImageOpenNode'";
    if (found)
    {
        const char* friendly = flow_factory_get_friendly_name(factory_, image_open_class.c_str());
        ASSERT_NE(friendly, nullptr);
        EXPECT_STREQ(friendly, "ImageOpen");
    }
}

// (2) Compute reads a real file and emits an Encoded flow::Image whose
//     bytes match the file content.
TEST_F(ImageOpenNodeTest, ComputeEmitsEncodedImageWithFileBytes)
{
    const std::vector<uint8_t> payload = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 'h', 'e', 'l', 'l', 'o'};
    const std::string path             = write_temp_file(payload, ".png");

    // Find the registered ImageOpen class name (carries the C++ namespace).
    char** classes    = nullptr;
    std::size_t count = 0;
    ASSERT_EQ(flow_factory_get_node_classes(factory_, "Editor", &classes, &count), FLOW_SUCCESS);
    std::string image_open_class;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (std::string(classes[i]).find("ImageOpenNode") != std::string::npos)
        {
            image_open_class = classes[i];
        }
    }
    flow_free_string_array(classes, count);
    ASSERT_FALSE(image_open_class.empty());

    FlowNodeHandle node = flow_graph_add_node(graph_, image_open_class.c_str(), "loader");
    ASSERT_NE(node, nullptr);

    FlowNodeDataHandle path_h = flow_data_create_string(path.c_str());
    ASSERT_NE(path_h, nullptr);
    ASSERT_EQ(flow_node_set_input_data(node, "path", path_h), FLOW_SUCCESS);
    ASSERT_EQ(flow_node_invoke_compute(node), FLOW_SUCCESS);

    FlowNodeDataHandle out = flow_node_get_output_data(node, "image");
    ASSERT_NE(out, nullptr) << "ImageOpen produced no output";
    EXPECT_TRUE(flow_data_is_image(out));

    FlowImageDescriptor desc{};
    ASSERT_EQ(flow_data_image_borrow(out, &desc), FLOW_SUCCESS);
    EXPECT_EQ(desc.kind, FLOW_IMAGE_KIND_ENCODED);
    EXPECT_EQ(desc.bytes_length, payload.size());
    ASSERT_NE(desc.bytes, nullptr);
    EXPECT_EQ(std::memcmp(desc.bytes, payload.data(), payload.size()), 0);

    flow_release_handle(out);
    flow_release_handle(path_h);
    std::remove(path.c_str());
}

// (3) Empty path string: Compute is a no-op (no output set, no crash).
TEST_F(ImageOpenNodeTest, EmptyPathIsHandled)
{
    char** classes    = nullptr;
    std::size_t count = 0;
    ASSERT_EQ(flow_factory_get_node_classes(factory_, "Editor", &classes, &count), FLOW_SUCCESS);
    std::string image_open_class;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (std::string(classes[i]).find("ImageOpenNode") != std::string::npos)
        {
            image_open_class = classes[i];
        }
    }
    flow_free_string_array(classes, count);
    ASSERT_FALSE(image_open_class.empty());

    FlowNodeHandle node = flow_graph_add_node(graph_, image_open_class.c_str(), "loader");
    ASSERT_NE(node, nullptr);

    FlowNodeDataHandle path_h = flow_data_create_string("");
    ASSERT_NE(path_h, nullptr);
    ASSERT_EQ(flow_node_set_input_data(node, "path", path_h), FLOW_SUCCESS);

    // Compute returns success (the framework caught the no-op) but no
    // output should land.
    flow_node_invoke_compute(node);
    FlowNodeDataHandle out = flow_node_get_output_data(node, "image");
    EXPECT_EQ(out, nullptr) << "Empty path must not produce an output";

    flow_release_handle(path_h);
}

// (4) Missing file: Compute raises through the framework; no output set,
//     no crash.  We verify by reading the output port — it must be null.
TEST_F(ImageOpenNodeTest, MissingFileFailsCleanly)
{
    char** classes    = nullptr;
    std::size_t count = 0;
    ASSERT_EQ(flow_factory_get_node_classes(factory_, "Editor", &classes, &count), FLOW_SUCCESS);
    std::string image_open_class;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (std::string(classes[i]).find("ImageOpenNode") != std::string::npos)
        {
            image_open_class = classes[i];
        }
    }
    flow_free_string_array(classes, count);
    ASSERT_FALSE(image_open_class.empty());

    FlowNodeHandle node = flow_graph_add_node(graph_, image_open_class.c_str(), "loader");
    ASSERT_NE(node, nullptr);

    FlowNodeDataHandle path_h = flow_data_create_string("/tmp/this/path/should/never/exist.png");
    ASSERT_NE(path_h, nullptr);
    ASSERT_EQ(flow_node_set_input_data(node, "path", path_h), FLOW_SUCCESS);

    // InvokeCompute() catches the exception via Node::InvokeCompute's
    // catch-all (see flow-core); the framework reports an error.  We
    // only assert that no output snuck onto the port.
    flow_node_invoke_compute(node);
    FlowNodeDataHandle out = flow_node_get_output_data(node, "image");
    EXPECT_EQ(out, nullptr) << "Missing file must not produce an output";

    flow_release_handle(path_h);
}

} // namespace
