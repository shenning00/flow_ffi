// test_flow_output_image.cpp — P15: FlowOutputImage sink node tests.
//
// Covers: factory registration, port layout, channel property, and
// Compute() no-op with no input.

#include "flow_ffi.h"

#include <flow/core/Env.hpp>
#include <flow/core/IndexableName.hpp>
#include <flow/core/NodeData.hpp>
#include <flow/core/NodeFactory.hpp>
#include <flow/core/UUID.hpp>

#include <wire/FlowOutputImage.hpp>
#include <wire/Image.hpp>
#include <wire/Register.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace
{

static std::shared_ptr<flow::Env> make_env()
{
    auto factory = std::make_shared<flow::NodeFactory>();
    flow::Settings settings;
    settings.MaxThreads = 1;
    return flow::Env::Create(factory, settings);
}

static std::shared_ptr<flow::Env> make_env_with_registry()
{
    auto env = make_env();
    wire::RegisterAllNodes(*env);
    return env;
}

} // namespace

class FlowOutputImageTest : public ::testing::Test
{
  protected:
    void SetUp() override { flow_clear_error(); }
    void TearDown() override { flow_clear_error(); }
};

// ---------------------------------------------------------------------------
// Scenario 1: Class is registered under "Editor" / "OutputImage"
// ---------------------------------------------------------------------------
TEST_F(FlowOutputImageTest, ClassIsRegisteredInFactory)
{
    auto env     = make_env_with_registry();
    auto factory = env->GetFactory();
    ASSERT_NE(factory, nullptr);

    const auto& cat_map = factory->GetCategories();
    const std::string expected_class{flow::TypeName_v<flow::FlowOutputImage>};

    bool found = false;
    auto range = cat_map.equal_range("Editor");
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second == expected_class)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "\"" << expected_class << "\" should be in the \"Editor\" category";

    // Verify friendly name via factory helper.
    EXPECT_EQ(factory->GetFriendlyName(expected_class), "OutputImage");
}

// ---------------------------------------------------------------------------
// Scenario 2: Input port "in" has type flow::Image; "channel" is std::string
// ---------------------------------------------------------------------------
TEST_F(FlowOutputImageTest, PortLayoutIsCorrect)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowOutputImage node(uuid, "out_img_test", env);

    const auto& inputs = node.GetInputPorts();
    ASSERT_EQ(inputs.size(), 2u) << "Expected exactly two input ports";

    auto in_it = inputs.find(flow::IndexableName{"in"});
    ASSERT_NE(in_it, inputs.end()) << "Input port \"in\" not found";
    EXPECT_EQ(in_it->second->GetDataType(), flow::TypeName_v<flow::Image>);

    auto ch_it = inputs.find(flow::IndexableName{"channel"});
    ASSERT_NE(ch_it, inputs.end()) << "Input port \"channel\" not found";
    EXPECT_EQ(ch_it->second->GetDataType(), flow::TypeName_v<std::string>);

    // FlowOutputImage emits a Flutter texture_id (int64) on its sole output port.
    // On Linux, fl_texture_get_id returns reinterpret_cast<int64_t>(self), so this
    // value is pointer-shaped — that is the legitimate Flutter Linux convention,
    // not a bug.  See ISSUE_6.md for the misdiagnosis history.
    const auto& outputs = node.GetOutputPorts();
    ASSERT_EQ(outputs.size(), 1u) << "Expected exactly one output port";
    auto tex_it = outputs.find(flow::IndexableName{"texture_id"});
    ASSERT_NE(tex_it, outputs.end()) << "Output port \"texture_id\" not found";
    EXPECT_EQ(tex_it->second->GetDataType(), flow::TypeName_v<std::int64_t>);
}

// ---------------------------------------------------------------------------
// Scenario 3: channel default is "" and is settable
// ---------------------------------------------------------------------------
TEST_F(FlowOutputImageTest, ChannelDefaultAndSettable)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowOutputImage node(uuid, "out_img_ch", env);

    // Default: no data on the channel port (nullptr data = default empty).
    auto ch_data = node.GetInputData<std::string>(flow::IndexableName{"channel"});
    // Either nullptr (no data set) or the empty string.
    if (ch_data)
    {
        EXPECT_EQ(ch_data->Get(), "");
    }

    // Settable without throwing.
    EXPECT_NO_THROW(node.SetInputData(flow::IndexableName{"channel"},
                                      std::make_shared<flow::NodeData<std::string>>(std::string{"preview"}),
                                      /*compute=*/false));

    auto ch_after = node.GetInputData<std::string>(flow::IndexableName{"channel"});
    ASSERT_NE(ch_after, nullptr);
    EXPECT_EQ(ch_after->Get(), "preview");
}

// ---------------------------------------------------------------------------
// Scenario 4: Compute() with no input is a no-op — no crash, no error
// ---------------------------------------------------------------------------
TEST_F(FlowOutputImageTest, ComputeWithNoInputIsNoOp)
{
    auto env = make_env();
    flow::UUID uuid;
    flow::FlowOutputImage node(uuid, "out_img_noop", env);

    bool error_fired = false;
    node.OnError.Bind(flow::IndexableName{"err_listener"}, [&](const std::exception&) { error_fired = true; });

    EXPECT_NO_THROW(node.InvokeCompute());
    EXPECT_FALSE(error_fired);
}
