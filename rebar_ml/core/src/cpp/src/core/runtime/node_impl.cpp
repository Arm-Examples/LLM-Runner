//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "core/runtime_internal.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_set>

namespace rebar_ml::internal {
namespace {

template <typename T>
std::shared_ptr<T> at_or_null(const std::vector<std::shared_ptr<T>>& values, std::size_t index) {
    return index < values.size() ? values[index] : nullptr;
}

}  // namespace

std::shared_ptr<InputPortAny> NodeImpl::get_input_port_any(PortId port_id) const {
    return at_or_null(inputs_, port_id);
}

std::shared_ptr<OutputPortAny> NodeImpl::get_output_port_any(PortId port_id) const {
    return at_or_null(outputs_, port_id);
}

const std::vector<std::shared_ptr<InputPortAny>>& NodeImpl::input_ports() const noexcept {
    return inputs_;
}

const std::vector<std::shared_ptr<OutputPortAny>>& NodeImpl::output_ports() const noexcept {
    return outputs_;
}

std::shared_ptr<IInputPortAny> NodeImpl::get_input_port(TypeId type) {
    const auto type_index = type_id_index(type);
    if (type_index >= defaultTypeIdToInputPortLookup_.size()) return nullptr;
    const auto port_id = defaultTypeIdToInputPortLookup_[type_index];
    return port_id == kInvalidPortId ? nullptr : get_input_port_by_id(port_id);
}

std::shared_ptr<IOutputPortAny> NodeImpl::get_output_port(TypeId type) {
    const auto type_index = type_id_index(type);
    if (type_index >= defaultTypeIdToOutputPortLookup_.size()) return nullptr;
    const auto port_id = defaultTypeIdToOutputPortLookup_[type_index];
    return port_id == kInvalidPortId ? nullptr : get_output_port_by_id(port_id);
}

std::shared_ptr<IInputPortAny> NodeImpl::get_input_port(TypeId type, std::size_t type_index) {
    const auto id = type_id_index(type);
    if (id >= inputs_by_type_.size()) return nullptr;
    const auto& ports = inputs_by_type_[id];
    if (type_index >= ports.size()) return nullptr;
    return get_input_port_by_id(ports[type_index]);
}

std::shared_ptr<IOutputPortAny> NodeImpl::get_output_port(TypeId type, std::size_t type_index) {
    const auto id = type_id_index(type);
    if (id >= outputs_by_type_.size()) return nullptr;
    const auto& ports = outputs_by_type_[id];
    if (type_index >= ports.size()) return nullptr;
    return get_output_port_by_id(ports[type_index]);
}

std::shared_ptr<IInputPortAny> NodeImpl::get_input_port_by_id(PortId port_id) {
    return std::dynamic_pointer_cast<IInputPortAny>(get_input_port_any(port_id));
}

std::shared_ptr<IOutputPortAny> NodeImpl::get_output_port_by_id(PortId port_id) {
    return std::dynamic_pointer_cast<IOutputPortAny>(get_output_port_any(port_id));
}

Result<std::shared_ptr<IPipelineSession>> NodeImpl::create_pipeline_session(
    const PipelineSessionOptions& options) {
    auto pipeline = PipelineImpl::create(shared_from_this(), options);
    if (!pipeline) return unexpected(pipeline.error());
    return std::static_pointer_cast<IPipelineSession>(*pipeline);
}

std::vector<std::shared_ptr<NodeImpl>> NodeImpl::neighbors() const {
    std::vector<std::shared_ptr<NodeImpl>> result;
    auto add = [&result](const std::shared_ptr<NodeImpl>& node) {
        if (node && std::none_of(result.begin(), result.end(), [&](const auto& current) {
                return current.get() == node.get();
            })) {
            result.push_back(node);
        }
    };

    for (const auto& port : inputs_) add(port->upstream_node());
    for (const auto& port : outputs_) add(port->downstream_node());
    return result;
}

std::vector<std::shared_ptr<NodeImpl>> NodeImpl::downstream_nodes() const {
    std::vector<std::shared_ptr<NodeImpl>> result;
    auto add = [&result](const std::shared_ptr<NodeImpl>& node) {
        if (node && std::none_of(result.begin(), result.end(), [&](const auto& current) {
                return current.get() == node.get();
            })) {
            result.push_back(node);
        }
    };
    for (const auto& port : outputs_) add(port->downstream_node());
    return result;
}

std::size_t NodeImpl::upstream_node_count() const noexcept {
    std::unordered_set<NodeImpl*> upstream;
    auto add = [&upstream](const std::shared_ptr<NodeImpl>& node) {
        if (node) upstream.insert(node.get());
    };
    for (const auto& port : inputs_) add(port->upstream_node());
    return upstream.size();
}

}  // namespace rebar_ml::internal
