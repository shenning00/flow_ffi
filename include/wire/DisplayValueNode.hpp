#pragma once

// DisplayValueNode — generic display sink that holds the latest value
// arriving on its single std::any-typed input "value".  Compute() is
// intentionally empty: the frontend (Flutter widget body, ImGui panel, …)
// renders by observing the input notifier directly.
//
// Pairs with a Dart-side visual prototype that declares a read-only
// "displayed" field; the Flutter FlowBridge's generalized _wireDisplayField
// helper subscribes to every input port on any node carrying a "displayed"
// field, stringifies the inbound value, and pushes it into the field.
// Result: dragging a wire from any primitive-typed output (int / float /
// bool / string) into this node renders the live value in the body — no
// per-type display node needed.
//
// Lives in wire (alongside PreviewNode) because it's frontend-agnostic
// and only depends on flow-core's wire-type machinery.

#include <flow/core/Env.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <any>
#include <memory>
#include <string>
#include <utility>

FLOW_NAMESPACE_BEGIN

class DisplayValueNode;
template<>
constexpr std::string_view TypeName_v<DisplayValueNode> = "DisplayValueNode";

class DisplayValueNode : public Node
{
  public:
    explicit DisplayValueNode(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<DisplayValueNode>, name, std::move(env))
    {
        AddInput<std::any>("value", "");
    }

    ~DisplayValueNode() override = default;

  protected:
    void Compute() override
    {
        // Sink — the frontend renders by observing the input notifier.
    }
};

FLOW_NAMESPACE_END
