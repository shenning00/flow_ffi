#pragma once

// node_wrapper.hpp — single authoritative definition of NodeWrapper.
//
// NodeWrapper must be defined in exactly ONE header included by every TU that
// creates or looks up node handles.  Defining it locally in multiple .cpp
// files is an ODR violation; more importantly, with per-TU definitions the
// dynamic_cast inside HandleRegistry::get_handle<NodeWrapper> can fail when
// the create-site TU and the lookup-site TU differ, because each TU would
// produce its own std::type_info object for Handle<NodeWrapper> and
// dynamic_cast compares those by pointer identity before falling back to
// name-string comparison.  A single header guarantees one definition, one
// type_info, and correct cross-TU casts inside the same DSO.

#include <flow/core/Node.hpp>
#include <memory>

using flow::SharedNode;

struct NodeWrapper {
    SharedNode node;
    NodeWrapper(SharedNode n) : node(std::move(n)) {}
};
