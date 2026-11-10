//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "rebar/core.hpp"

#include <atomic>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace rebar_ml::internal {

template <typename T>
class InputPortImpl;

template <typename T>
class OutputPortImpl;

class InputPortAny;
class OutputPortAny;

class PipelineImpl;
class NodeRuntime;

class NodeImpl
    : public INode,
      public std::enable_shared_from_this<NodeImpl> {
public:
    explicit NodeImpl(std::string name) : name_(std::move(name)) {
        defaultTypeIdToInputPortLookup_.fill(kInvalidPortId);
        defaultTypeIdToOutputPortLookup_.fill(kInvalidPortId);
    }
    ~NodeImpl() override = default;

    std::string_view get_name() const noexcept override { return name_; }

    std::size_t input_port_count() const noexcept override { return inputs_.size(); }
    std::size_t output_port_count() const noexcept override { return outputs_.size(); }

    using INode::get_input_port;
    using INode::get_output_port;

    std::shared_ptr<IInputPortAny> get_input_port(TypeId type) override;
    std::shared_ptr<IOutputPortAny> get_output_port(TypeId type) override;
    std::shared_ptr<IInputPortAny> get_input_port(TypeId type, std::size_t type_index) override;
    std::shared_ptr<IOutputPortAny> get_output_port(TypeId type, std::size_t type_index) override;
    std::shared_ptr<IInputPortAny> get_input_port_by_id(PortId port_id) override;
    std::shared_ptr<IOutputPortAny> get_output_port_by_id(PortId port_id) override;

    Result<std::shared_ptr<IPipelineSession>> create_pipeline_session(
        const PipelineSessionOptions& options = {}) override;

    std::shared_ptr<InputPortAny> get_input_port_any(PortId port_id) const;
    std::shared_ptr<OutputPortAny> get_output_port_any(PortId port_id) const;
    const std::vector<std::shared_ptr<InputPortAny>>& input_ports() const noexcept;
    const std::vector<std::shared_ptr<OutputPortAny>>& output_ports() const noexcept;

    std::vector<std::shared_ptr<NodeImpl>> neighbors() const;
    std::vector<std::shared_ptr<NodeImpl>> downstream_nodes() const;
    std::size_t upstream_node_count() const noexcept;

    bool graph_in_use() const noexcept { return active_pipelines_.load() != 0; }
    void retain_pipeline() noexcept { active_pipelines_.fetch_add(1); }
    void release_pipeline() noexcept { active_pipelines_.fetch_sub(1); }

    virtual std::unique_ptr<NodeRuntime> create_runtime(PipelineImpl& pipeline) = 0;

protected:
    template <typename T>
    std::shared_ptr<InputPortImpl<T>> add_input();

    template <typename T>
    std::shared_ptr<OutputPortImpl<T>> add_output();

private:
    std::string name_;
    std::atomic<std::size_t> active_pipelines_{0};

    std::vector<std::shared_ptr<InputPortAny>> inputs_;
    std::vector<std::shared_ptr<OutputPortAny>> outputs_;
    std::array<std::vector<PortId>, kTypeIdCount> inputs_by_type_{};
    std::array<std::vector<PortId>, kTypeIdCount> outputs_by_type_{};
    std::array<PortId, kTypeIdCount> defaultTypeIdToInputPortLookup_{};
    std::array<PortId, kTypeIdCount> defaultTypeIdToOutputPortLookup_{};

    friend class PipelineImpl;
};

class NodeRuntime {
public:
    virtual ~NodeRuntime() = default;

    virtual Result<void> on_payload(PortId port, AnyPayload payload) = 0;

    virtual Result<void> finish() { return {}; }
    virtual void cancel() noexcept {}
};

}  // namespace rebar_ml::internal
