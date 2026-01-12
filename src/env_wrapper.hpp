#pragma once

#include <flow/core/Env.hpp>
#include <flow/core/NodeFactory.hpp>

#include <memory>

// Shared wrapper structures for Environment and NodeFactory

struct EnvWrapper {
    std::shared_ptr<flow::Env> env;

    EnvWrapper(std::shared_ptr<flow::Env> e) : env(std::move(e)) {}
};

struct NodeFactoryWrapper {
    std::shared_ptr<flow::NodeFactory> factory;

    NodeFactoryWrapper(std::shared_ptr<flow::NodeFactory> f) : factory(std::move(f)) {}
};