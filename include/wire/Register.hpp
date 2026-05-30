#pragma once

// wire registration entry point.  An FFI/frontend binding wishing to
// surface wire's builtin and (optionally) OpenCV adapter nodes calls
// `wire::RegisterAllNodes(env)` once during environment setup —
// typically inside the binding's equivalent of `flow_env_create`.
//
// When wire was built with FLOW_INTERWORK_WITH_OPENCV the function
// also registers the cv::Mat ↔ flow::Image adapter pair.  Without the
// macro it registers only the always-present nodes (ImageOpenNode,
// PreviewNode).  The decision is fixed at compile time — graphs persisted
// against a build that included the adapters will fail to load on a build
// that did not, with the same missing-node error any unloaded module
// would produce.

#include <flow/core/Env.hpp>

namespace wire
{

// Register every node type that wire contributes to the given Env's
// factory.  Safe to call more than once (NodeFactory ignores duplicate
// registrations); typically called exactly once at env init.
void RegisterAllNodes(flow::Env& env);

} // namespace wire
