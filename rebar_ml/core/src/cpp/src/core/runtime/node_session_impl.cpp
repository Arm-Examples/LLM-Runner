//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "node_session_impl.hpp"

#include "core/runtime_internal.hpp"
#include "actor.hpp"

#include <cassert>
#include <optional>
#include <type_traits>
#include <vector>

namespace rebar_ml::internal {

namespace {

class AnyInputStreamImpl final
    : public IInputStreamAny,
      public StagedInput,
      public std::enable_shared_from_this<AnyInputStreamImpl> {
public:
    AnyInputStreamImpl(
        std::shared_ptr<PipelineImpl> pipeline,
        TypeId type,
        PortId input_port_id,
        NodeContext* node_context,
        std::shared_ptr<Actor> actor)
        : pipeline_(std::move(pipeline)),
          type_(type),
          input_port_id_(input_port_id),
          context_(node_context),
          actor_(std::move(actor)) {}

    TypeId get_type_id() const noexcept override { return type_; }

    Result<void> add_payload(AnyPayload payload) override {
        auto open = pipeline_->ensure_input_open();
        if (!open) return unexpected(open.error());
#ifndef NDEBUG
        if (producer_thread_ == std::thread::id{}) {
            producer_thread_ = std::this_thread::get_id();
        } else {
            assert(producer_thread_ == std::this_thread::get_id() &&
                "AnyInputStreamImpl add_payload() called from multiple threads");
        }
#endif
        if (!payload || payload.type_id() != type_) {
            return unexpected(Error{
                ErrorCode::invalid_argument,
                "input payload type does not match input stream type",
            });
        }
        staged_.push_back(std::move(payload));
        return {};
    }

    Result<void> send_payload(AnyPayload payload) override {
        auto open = pipeline_->ensure_input_open();
        if (!open) return unexpected(open.error());
#ifndef NDEBUG
        if (producer_thread_ == std::thread::id{}) {
            producer_thread_ = std::this_thread::get_id();
        } else {
            assert(producer_thread_ == std::this_thread::get_id() &&
                "AnyInputStreamImpl send_payload() called from multiple threads");
        }
#endif
        if (!payload || payload.type_id() != type_) {
            return unexpected(Error{
                ErrorCode::invalid_argument,
                "input payload type does not match input stream type",
            });
        }
        if (pipeline_->has_pending_input()) {
            return unexpected(Error{
                ErrorCode::input_pending,
                "send() cannot be used while a staged pipeline submission is pending",
            });
        }
        return dispatch(std::move(payload));
    }

    bool has_pending_data() const noexcept override { return has_pending(); }
    bool has_pending() const noexcept override {
        return !staged_.empty();
    }

    Result<void> flush_staged() override {
        std::vector<AnyPayload> payloads;
        payloads.swap(staged_);
        for (auto& payload : payloads) {
            auto result = dispatch(std::move(payload));
            if (!result) return result;
        }
        return {};
    }

private:
    Result<void> dispatch(AnyPayload payload) {
        auto* pipeline = pipeline_.get();
        auto input_port_id = input_port_id_;
        auto* context = context_;
        auto boxed_payload = std::make_shared<AnyPayload>(std::move(payload));
        auto token = pipeline_->acquire_pending_token();
        if (!actor_->post([pipeline, context, input_port_id, boxed_payload, token]() mutable {
            (void)pipeline->deliver_payload(context, input_port_id, std::move(*boxed_payload));
        })) {
            return unexpected(Error{ErrorCode::cancelled, "node actor is closed"});
        }
        return {};
    }

    std::shared_ptr<PipelineImpl> pipeline_;
    TypeId type_{};
    PortId input_port_id_;
    NodeContext* context_ = nullptr;
    std::shared_ptr<Actor> actor_;
    std::vector<AnyPayload> staged_;
#ifndef NDEBUG
    std::thread::id producer_thread_;
#endif
};

class AnyOutputStreamImpl final : public IOutputStreamAny {
public:
    AnyOutputStreamImpl(
        TypeId type,
        std::shared_ptr<TerminalQueue<AnyPayload>> queue)
        : type_(type), queue_(std::move(queue)) {}

    TypeId get_type_id() const noexcept override { return type_; }

    OutputRetrieval<AnyPayload> receive_payload() override {
        auto received = queue_->receive();
        if (received.status != RetrievalStatus::success) return received;
        if (received.item && received.item->data.type_id() != type_) {
            return OutputRetrieval<AnyPayload>::failure(Error{
                ErrorCode::invalid_graph,
                "output item type does not match output port type",
            });
        }
        return received;
    }

    void cancel() noexcept override { queue_->cancel(); }

private:
    TypeId type_{};
    std::shared_ptr<TerminalQueue<AnyPayload>> queue_;
};

}  // namespace

NodeSessionImpl::NodeSessionImpl(std::shared_ptr<PipelineImpl> pipeline, std::shared_ptr<NodeImpl> node)
    : pipeline_(std::move(pipeline)),
      node_(std::move(node)),
      context_(pipeline_->context_for(node_.get())),
      actor_(pipeline_->actor_for(context_)),
      inputs_(node_->input_port_count()),
      outputs_(node_->output_port_count()) {
    assert(actor_ && "node actor is unavailable");

    terminal_inputs_by_type_.fill(kInvalidPortId);
    for (const auto& port : node_->input_ports()) {
        if (!port || port->is_connected_any()) continue;
        const auto index = type_id_index(port->type_id());
        if (index >= terminal_inputs_by_type_.size()) continue;
        if (terminal_inputs_by_type_[index] == kInvalidPortId) terminal_inputs_by_type_[index] = port->port_index();
    }

    terminal_outputs_by_type_.fill(kInvalidPortId);
    for (const auto& port : node_->output_ports()) {
        if (!port || port->is_connected_any()) continue;
        const auto index = type_id_index(port->type_id());
        if (index >= terminal_outputs_by_type_.size()) continue;
        if (terminal_outputs_by_type_[index] == kInvalidPortId) terminal_outputs_by_type_[index] = port->port_index();
    }
}

std::shared_ptr<INode> NodeSessionImpl::get_node() const {
    return node_;
}

std::shared_ptr<IInputStreamAny> NodeSessionImpl::get_input(TypeId type) {
    pipeline_->debug_assert_pipeline_thread();
    const auto index = type_id_index(type);
    if (index >= terminal_inputs_by_type_.size()) return nullptr;
    const auto port_id = terminal_inputs_by_type_[index];
    return port_id == kInvalidPortId ? nullptr : get_input_by_id(port_id);
}

std::shared_ptr<IOutputStreamAny> NodeSessionImpl::get_output(TypeId type) {
    pipeline_->debug_assert_pipeline_thread();
    const auto index = type_id_index(type);
    if (index >= terminal_outputs_by_type_.size()) return nullptr;
    const auto port_id = terminal_outputs_by_type_[index];
    return port_id == kInvalidPortId ? nullptr : get_output_by_id(port_id);
}

std::shared_ptr<IInputStreamAny> NodeSessionImpl::get_input_by_id(PortId port_id) {
    pipeline_->debug_assert_pipeline_thread();
    if (port_id >= inputs_.size()) return nullptr;
    auto port = node_->get_input_port_any(port_id);
    if (!port || port->is_connected_any()) return nullptr;

    auto& stream = inputs_[port_id];
    if (!stream) {
        auto created = std::make_shared<AnyInputStreamImpl>(
            pipeline_, port->type_id(), port_id, context_, actor_);
        pipeline_->register_staged_input(created);
        stream = std::move(created);
    }
    return stream;
}

std::shared_ptr<IOutputStreamAny> NodeSessionImpl::get_output_by_id(PortId port_id) {
    pipeline_->debug_assert_pipeline_thread();
    if (port_id >= outputs_.size()) return nullptr;
    auto port = node_->get_output_port_any(port_id);
    if (!port || port->is_connected_any()) return nullptr;

    auto& stream = outputs_[port_id];
    if (!stream) {
        auto queue = pipeline_->output_queue(context_, port_id);
        if (!queue) return nullptr;
        stream = std::make_shared<AnyOutputStreamImpl>(
            port->type_id(),
            std::move(queue));
    }
    return stream;
}

}  // namespace rebar_ml::internal
