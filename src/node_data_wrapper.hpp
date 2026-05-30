#pragma once

// node_data_wrapper.hpp — single authoritative definition of NodeDataWrapper.
//
// NodeDataWrapper must be defined in exactly ONE header included by every TU
// that creates or looks up node-data handles.  Defining it locally in multiple
// .cpp files is an ODR violation; more importantly, with per-TU definitions
// the dynamic_cast inside HandleRegistry::get_handle<NodeDataWrapper> can fail
// when the create-site TU and the lookup-site TU differ, because each TU
// would produce its own std::type_info object for Handle<NodeDataWrapper> and
// dynamic_cast compares those by pointer identity before falling back to
// name-string comparison.  A single header guarantees one definition, one
// type_info, and correct cross-TU casts inside the same DSO.
//
// Same ODR rationale as node_wrapper.hpp (the sibling header for NodeWrapper).

#include <flow/core/NodeData.hpp>
#include <memory>

using flow::SharedNodeData;

struct NodeDataWrapper
{
    SharedNodeData data;
    NodeDataWrapper(SharedNodeData d) : data(std::move(d)) {}
};
