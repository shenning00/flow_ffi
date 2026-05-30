#pragma once

// PreviewNode — sink node that holds the latest payload arriving on its
// single std::any-typed input "in".  Compute() is intentionally empty: the
// frontend (Flutter widget, ImGui panel, …) renders by observing the input
// notifier directly, so this node only needs to exist so graphs can wire to
// it.
//
// The std::any port type is the "accept anything" marker — type-compat
// enforcement happens on the frontend side.  Lives in `wire` because
// it has zero frontend-specific code; the rendering side is a separate
// concern owned by whichever binding consumes it.

#include <flow/core/Env.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <any>
#include <memory>
#include <string>
#include <utility>

FLOW_NAMESPACE_BEGIN

class PreviewNode;
template<>
constexpr std::string_view TypeName_v<PreviewNode> = "PreviewNode";

class PreviewNode : public Node
{
  public:
    explicit PreviewNode(const UUID& uuid, const std::string& name, std::shared_ptr<Env> env)
        : Node(uuid, TypeName_v<PreviewNode>, name, std::move(env))
    {
        AddInput<std::any>("in", "");
    }

    ~PreviewNode() override = default;

  protected:
    void Compute() override
    {
        // Sink — the frontend renders by observing the input notifier.
    }
};

FLOW_NAMESPACE_END
