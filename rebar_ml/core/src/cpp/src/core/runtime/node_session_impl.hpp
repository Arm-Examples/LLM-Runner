//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "rebar/core.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace rebar_ml::internal {

class NodeImpl;
class PipelineImpl;
class Actor;
struct NodeContext;
template <typename T>
class TerminalQueue;

class NodeSessionImpl final
    : public INodeSession,
      public std::enable_shared_from_this<NodeSessionImpl> {
public:
    NodeSessionImpl(std::shared_ptr<PipelineImpl> pipeline, std::shared_ptr<NodeImpl> node);
    ~NodeSessionImpl() override = default;

    std::shared_ptr<INode> get_node() const override;

    using INodeSession::get_input;
    using INodeSession::get_output;

    std::shared_ptr<IInputStreamAny> get_input(TypeId type) override;
    std::shared_ptr<IOutputStreamAny> get_output(TypeId type) override;
    std::shared_ptr<IInputStreamAny> get_input_by_id(PortId port_id) override;
    std::shared_ptr<IOutputStreamAny> get_output_by_id(PortId port_id) override;

private:
    std::shared_ptr<PipelineImpl> pipeline_;
    std::shared_ptr<NodeImpl> node_;
    NodeContext* context_ = nullptr;
    std::shared_ptr<Actor> actor_;
    std::vector<std::shared_ptr<IInputStreamAny>> inputs_;
    std::vector<std::shared_ptr<IOutputStreamAny>> outputs_;
    std::array<PortId, kTypeIdCount> terminal_inputs_by_type_{};
    std::array<PortId, kTypeIdCount> terminal_outputs_by_type_{};
};

}  // namespace rebar_ml::internal
