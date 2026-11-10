//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "actor.hpp"
#include "node_impl.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <type_traits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace rebar_ml::internal {

struct NodeContext;
struct ConnectionState;

class StagedInput {
public:
    virtual ~StagedInput() = default;
    virtual bool has_pending() const noexcept = 0;
    virtual Result<void> flush_staged() = 0;
};

template <typename T>
class TerminalQueue {
public:
    void push(OutputItem<T> item) {
        std::lock_guard lock(mutex_);
        if (terminal_) return;
        items_.push_back(std::move(item));
        cv_.notify_one();
    }

    void finish() {
        std::lock_guard lock(mutex_);
        if (terminal_) return;
        terminal_ = true;
        cv_.notify_all();
    }

    void fail(Error error) {
        std::lock_guard lock(mutex_);
        if (terminal_) return;
        error_ = std::move(error);
        terminal_ = true;
        cv_.notify_all();
    }

    void cancel() {
        std::lock_guard lock(mutex_);
        if (terminal_) return;
        cancelled_ = true;
        terminal_ = true;
        items_.clear();
        cv_.notify_all();
    }

    OutputRetrieval<T> receive() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !items_.empty() || terminal_; });

        if (!items_.empty()) {
            auto item = std::move(items_.front());
            items_.pop_front();
            return OutputRetrieval<T>::success(std::move(item));
        }
        if (cancelled_) return OutputRetrieval<T>::cancelled_result();
        if (error_) return OutputRetrieval<T>::failure(*error_);
        return OutputRetrieval<T>::success(std::nullopt);
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<OutputItem<T>> items_;
    std::optional<Error> error_;
    bool terminal_ = false;
    bool cancelled_ = false;
};

class PipelineImpl
    : public IPipelineSession,
      public std::enable_shared_from_this<PipelineImpl> {
public:
    static Result<std::shared_ptr<PipelineImpl>> create(
        const std::shared_ptr<NodeImpl>& start,
        const PipelineSessionOptions& options);

    ~PipelineImpl() override;

    Result<std::shared_ptr<INodeSession>> get_node_session(
        const std::shared_ptr<INode>& node) override;

    void debug_assert_pipeline_thread() const;

    Result<void> submit() override;
    Result<void> finish_inputs() override;
    void teardown() noexcept override;
    void cancel() noexcept override;
    bool is_cancelled() const noexcept override { return cancelled_.load(); }

    NodeContext* context_for(NodeImpl* node) const;
    std::shared_ptr<Actor> actor_for(NodeContext* node) const;
    std::shared_ptr<void> acquire_pending_token();

    Result<void> deliver_payload(NodeContext* node, PortId input, AnyPayload payload);
    template <typename T>
    Result<void> deliver(NodeContext* node, PortId input, T data) {
        return deliver_payload(
            node,
            input,
            make_payload(std::move(data)));
    }

    Result<void> forward_payload(
        NodeContext* node, PortId output, AnyPayload payload, bool final);
    template <typename T>
    Result<void> forward(NodeContext* node, PortId output, T data, bool final) {
        return forward_payload(
            node,
            output,
            make_payload(std::move(data)),
            final);
    }

    std::shared_ptr<TerminalQueue<AnyPayload>> output_queue(
        NodeContext* node, PortId output) const;

    void register_staged_input(const std::shared_ptr<StagedInput>& input);
    bool has_pending_input() const;
    Result<void> ensure_input_open() const;
    void fail(const Error& error);

private:
    PipelineImpl() = default;
    Result<void> initialize(const std::shared_ptr<NodeImpl>& start);

    std::shared_ptr<TerminalQueue<AnyPayload>> ensure_output_queue(
        NodeContext* node, PortId output);

    Result<void> wait_for_idle();
    void drain_connection(const ConnectionState* connection);

    std::vector<std::shared_ptr<NodeImpl>> nodes_;
    std::vector<NodeImpl*> topological_order_;
    std::vector<NodeContext*> topological_contexts_;
    std::unordered_map<NodeImpl*, NodeContext*> contexts_;
    std::vector<std::unique_ptr<NodeContext>> owned_contexts_;
    std::unordered_map<NodeImpl*, std::weak_ptr<INodeSession>> sessions_;

    std::vector<std::weak_ptr<StagedInput>> staged_inputs_;

    std::atomic<bool> cancelled_{false};
    std::atomic<bool> inputs_finished_{false};
    std::atomic<Error*> terminal_error_{nullptr};
    std::atomic<std::size_t> pending_deliveries_{0};
    mutable std::mutex pending_mutex_;
    mutable std::condition_variable pending_cv_;

#ifndef NDEBUG
    std::thread::id owner_thread_ = std::this_thread::get_id();
#endif
};

}  // namespace rebar_ml::internal
