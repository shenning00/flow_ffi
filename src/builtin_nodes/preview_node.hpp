#pragma once

// preview_node.hpp — built-in sink node that the Dart editor binds to a
// PreviewNodeWidget (see FLOW_RUN.html §B.4.6).  Single std::any-typed
// input named "in", no outputs, empty Compute().  Mirrors flow-ui's
// EditorNodes.hpp PreviewNode so persisted graphs round-trip cleanly.
//
// Registered against the Env's factory under category "Editor" with
// friendly name "Preview" (see env_bridge.cpp).  The std::any port type
// is the "accept anything" marker used by flow-ui — the type-compatibility
// check on the Dart side permits any -> image and any -> primitive.

#include <flow/core/Env.hpp>
#include <flow/core/Node.hpp>
#include <flow/core/TypeName.hpp>
#include <flow/core/UUID.hpp>

#include <any>
#include <memory>
#include <string>
#include <utility>

namespace flow::ffi::builtin {

class PreviewNode : public flow::Node {
  public:
    explicit PreviewNode(const flow::UUID& uuid, const std::string& name,
                         std::shared_ptr<flow::Env> env)
        : flow::Node(uuid, flow::TypeName_v<PreviewNode>, name, std::move(env)) {
        AddInput<std::any>("in", "");
    }

    ~PreviewNode() override = default;

  protected:
    void Compute() override {
        // sink — preview is rendered by the Dart widget observing the
        // input notifier; no computation happens here.
    }
};

} // namespace flow::ffi::builtin
