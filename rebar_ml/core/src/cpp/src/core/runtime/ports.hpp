//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "node_impl.hpp"

#include <cstddef>
#include <memory>

namespace rebar_ml::internal {

class OutputPortAny;

class InputPortAny {
public:
    virtual ~InputPortAny() = default;
    std::shared_ptr<NodeImpl> owner_internal() const { return owner_->shared_from_this(); }
    TypeId type_id() const noexcept { return type_; }
    std::size_t port_index() const noexcept { return index_; }
    bool is_connected_any() const noexcept { return !upstream_.expired(); }
    std::shared_ptr<NodeImpl> upstream_node() const;

private:
    template <typename T>
    friend class InputPortImpl;
    template <typename T>
    friend class OutputPortImpl;

protected:
    InputPortAny(NodeImpl* owner, TypeId type, std::size_t index)
        : owner_(owner), type_(type), index_(index) {}

    void attach_upstream(const std::shared_ptr<OutputPortAny>& upstream) { upstream_ = upstream; }
    void detach_upstream() { upstream_.reset(); }

    NodeImpl* owner_ = nullptr;
    TypeId type_;
    std::size_t index_ = 0;
    std::weak_ptr<OutputPortAny> upstream_;
};

class OutputPortAny {
public:
    virtual ~OutputPortAny() = default;
    std::shared_ptr<NodeImpl> owner_internal() const { return owner_->shared_from_this(); }
    TypeId type_id() const noexcept { return type_; }
    std::size_t port_index() const noexcept { return index_; }
    bool is_connected_any() const noexcept { return !destination_.expired(); }
    std::shared_ptr<NodeImpl> downstream_node() const;
    std::size_t destination_index() const noexcept;
    std::shared_ptr<InputPortAny> destination_any() const { return destination_.lock(); }
    ConnectionOptions connection_options() const noexcept { return options_; }

private:
    template <typename T>
    friend class OutputPortImpl;

protected:
    OutputPortAny(NodeImpl* owner, TypeId type, std::size_t index)
        : owner_(owner), type_(type), index_(index) {}

    void attach_destination(const std::shared_ptr<InputPortAny>& destination) { destination_ = destination; }
    void detach_destination() { destination_.reset(); }

    NodeImpl* owner_ = nullptr;
    TypeId type_;
    std::size_t index_ = 0;
    std::weak_ptr<InputPortAny> destination_;
    ConnectionOptions options_;
};

template <typename T>
class OutputPortImpl;

template <typename T>
class InputPortImpl final : public IInputPort<T>, public InputPortAny {
public:
    InputPortImpl(NodeImpl* owner, std::size_t index)
        : InputPortAny(owner, type_id_of<T>(), index) {}

    std::shared_ptr<INode> get_owner() const override { return owner_internal(); }
    std::size_t get_index() const noexcept override { return port_index(); }
    bool is_connected() const noexcept override { return is_connected_any(); }
};

template <typename T>
class OutputPortImpl final
    : public IOutputPort<T>,
      public OutputPortAny,
      public std::enable_shared_from_this<OutputPortImpl<T>> {
public:
    OutputPortImpl(NodeImpl* owner, std::size_t index)
        : OutputPortAny(owner, type_id_of<T>(), index) {}

    std::shared_ptr<INode> get_owner() const override { return owner_internal(); }
    std::size_t get_index() const noexcept override { return port_index(); }
    bool is_connected() const noexcept override { return is_connected_any(); }

    Result<void> connect(
        const std::shared_ptr<IInputPort<T>>& destination,
        const ConnectionOptions& options) override;

    Result<void> disconnect() override;

    std::shared_ptr<InputPortImpl<T>> destination() const {
        return std::dynamic_pointer_cast<InputPortImpl<T>>(destination_any());
    }
};

inline std::shared_ptr<NodeImpl> InputPortAny::upstream_node() const {
    auto upstream = upstream_.lock();
    return upstream ? upstream->owner_internal() : nullptr;
}

template <typename T>
Result<void> OutputPortImpl<T>::connect(
    const std::shared_ptr<IInputPort<T>>& destination,
    const ConnectionOptions& options) {
    auto typed_destination = std::dynamic_pointer_cast<InputPortImpl<T>>(destination);
    if (!typed_destination) {
        return unexpected(Error{
            ErrorCode::incompatible_connection,
            "destination port is not compatible with this output port",
        });
    }
    if (owner_->graph_in_use() || typed_destination->owner_->graph_in_use()) {
        return unexpected(Error{
            ErrorCode::graph_in_use,
            "topology cannot be changed while a pipeline instance is alive",
        });
    }
    if (is_connected_any()) {
        return unexpected(Error{
            ErrorCode::already_connected,
            "output port is already connected; use a fan-out node for multiple consumers",
        });
    }
    if (typed_destination->is_connected_any()) {
        return unexpected(Error{
            ErrorCode::already_connected,
            "input port is already connected",
        });
    }
    if (options.queue_capacity == 0) {
        return unexpected(Error{
            ErrorCode::invalid_argument,
            "connection queue capacity must be greater than zero",
        });
    }

    attach_destination(typed_destination);
    typed_destination->attach_upstream(this->shared_from_this());
    options_ = options;
    return {};
}

template <typename T>
Result<void> OutputPortImpl<T>::disconnect() {
    if (owner_->graph_in_use()) {
        return unexpected(Error{
            ErrorCode::graph_in_use,
            "topology cannot be changed while a pipeline instance is alive",
        });
    }
    auto destination = destination_any();
    if (destination && destination->owner_->graph_in_use()) {
        return unexpected(Error{
            ErrorCode::graph_in_use,
            "topology cannot be changed while a pipeline instance is alive",
        });
    }
    if (destination) destination->detach_upstream();
    detach_destination();
    return {};
}

inline std::shared_ptr<NodeImpl> OutputPortAny::downstream_node() const {
    auto destination = destination_.lock();
    return destination ? destination->owner_internal() : nullptr;
}

inline std::size_t OutputPortAny::destination_index() const noexcept {
    auto destination = destination_.lock();
    return destination ? destination->port_index() : 0;
}

template <typename T>
std::shared_ptr<InputPortImpl<T>> NodeImpl::add_input() {
    const PortId port_id = inputs_.size();
    auto port = std::make_shared<InputPortImpl<T>>(this, port_id);
    inputs_.push_back(port);
    const auto type_index = type_id_index(port->type_id());
    inputs_by_type_[type_index].push_back(port_id);
    if (defaultTypeIdToInputPortLookup_[type_index] == kInvalidPortId) {
        defaultTypeIdToInputPortLookup_[type_index] = port_id;
    }
    return port;
}

template <typename T>
std::shared_ptr<OutputPortImpl<T>> NodeImpl::add_output() {
    const PortId port_id = outputs_.size();
    auto port = std::make_shared<OutputPortImpl<T>>(this, port_id);
    outputs_.push_back(port);
    const auto type_index = type_id_index(port->type_id());
    outputs_by_type_[type_index].push_back(port_id);
    if (defaultTypeIdToOutputPortLookup_[type_index] == kInvalidPortId) {
        defaultTypeIdToOutputPortLookup_[type_index] = port_id;
    }
    return port;
}

}  // namespace rebar_ml::internal
