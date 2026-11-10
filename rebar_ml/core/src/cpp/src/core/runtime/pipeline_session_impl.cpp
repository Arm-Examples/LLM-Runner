//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "core/runtime_internal.hpp"

#include "node_session_impl.hpp"

#include <algorithm>
#include <cassert>
#include <queue>
#include <unordered_set>

namespace rebar_ml::internal {

struct QueuedPayload {
    AnyPayload payload;
    std::shared_ptr<void> token;
};

class ConnectionQueue {
public:
    explicit ConnectionQueue(ConnectionOptions options) : options_(options) {}

    Result<bool> push(QueuedPayload value) {
        bool schedule = false;
        {
            std::unique_lock lock(mutex_);
            if (stopped_) {
                return unexpected(Error{ErrorCode::cancelled, "connection queue is closed"});
            }

            switch (options_.overflow) {
                case OverflowPolicy::block: {
                    not_full_.wait(lock, [&] { return stopped_ || items_.size() < options_.queue_capacity; });
                    if (stopped_) {
                        return unexpected(Error{ErrorCode::cancelled, "connection queue is closed"});
                    }
                    items_.push_back(std::move(value));
                    break;
                }
                case OverflowPolicy::fail: {
                    if (items_.size() >= options_.queue_capacity) {
                        return unexpected(Error{
                            ErrorCode::queue_full,
                            "connection queue is full",
                        });
                    }
                    items_.push_back(std::move(value));
                    break;
                }
                case OverflowPolicy::drop_oldest: {
                    if (items_.size() >= options_.queue_capacity && !items_.empty()) {
                        items_.pop_front();
                        not_full_.notify_one();
                    }
                    items_.push_back(std::move(value));
                    break;
                }
                case OverflowPolicy::drop_newest: {
                    if (items_.size() >= options_.queue_capacity) {
                        return false;
                    }
                    items_.push_back(std::move(value));
                    break;
                }
            }

            if (!drain_scheduled_ && !items_.empty()) {
                drain_scheduled_ = true;
                schedule = true;
            }
        }

        return schedule;
    }

    bool pop(QueuedPayload& out) {
        std::unique_lock lock(mutex_);
        if (items_.empty()) {
            drain_scheduled_ = false;
            return false;
        }
        out = std::move(items_.front());
        items_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return true;
    }

    void stop() {
        std::lock_guard lock(mutex_);
        stopped_ = true;
        drain_scheduled_ = false;
        items_.clear();
        not_full_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable not_full_;
    ConnectionOptions options_;
    std::deque<QueuedPayload> items_;
    bool stopped_ = false;
    bool drain_scheduled_ = false;
};

struct ConnectionState {
    NodeContext* destination = nullptr;
    TypeId type{};
    PortId destination_port = 0;
    std::shared_ptr<Actor> destination_actor;
    std::shared_ptr<ConnectionQueue> queue;

    ConnectionState(
        NodeContext* dest,
        TypeId type_value,
        PortId dest_port,
        std::shared_ptr<Actor> actor,
        ConnectionOptions options)
        : destination(dest),
          type(type_value),
          destination_port(dest_port),
          destination_actor(std::move(actor)),
          queue(std::make_shared<ConnectionQueue>(options)) {}
};

struct OutputRoute {
    TypeId type{};
    std::shared_ptr<TerminalQueue<AnyPayload>> terminal_queue;
    std::optional<ConnectionState> connection;
};

struct NodeContext {
    NodeImpl* node = nullptr;
    std::unique_ptr<NodeRuntime> runtime;
    std::shared_ptr<Actor> actor;
    std::vector<TypeId> input_types;
    std::vector<OutputRoute> outputs;
};

void PipelineImpl::debug_assert_pipeline_thread() const {
#ifndef NDEBUG
    assert(owner_thread_ == std::this_thread::get_id() && "PipelineImpl used from non-owner thread");
#endif
}

Result<std::shared_ptr<PipelineImpl>> PipelineImpl::create(
    const std::shared_ptr<NodeImpl>& start,
    const PipelineSessionOptions&) {
    if (!start) {
        return unexpected(Error{ErrorCode::invalid_argument, "start node is null"});
    }
    auto pipeline = std::shared_ptr<PipelineImpl>(new PipelineImpl());
    auto initialized = pipeline->initialize(start);
    if (!initialized) return unexpected(initialized.error());
    return pipeline;
}

NodeContext* PipelineImpl::context_for(NodeImpl* node) const {
    auto found = contexts_.find(node);
    return found == contexts_.end() ? nullptr : found->second;
}

std::shared_ptr<Actor> PipelineImpl::actor_for(NodeContext* node) const {
    return node ? node->actor : nullptr;
}

std::shared_ptr<void> PipelineImpl::acquire_pending_token() {
    pending_deliveries_.fetch_add(1, std::memory_order_acq_rel);
    PipelineImpl* pipeline = this;
    return std::shared_ptr<void>(nullptr, [pipeline](void*) {
        if (pipeline->pending_deliveries_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::lock_guard lock(pipeline->pending_mutex_);
            pipeline->pending_cv_.notify_all();
        }
    });
}

Result<void> PipelineImpl::initialize(const std::shared_ptr<NodeImpl>& start) {
    std::queue<std::shared_ptr<NodeImpl>> pending;
    std::unordered_set<NodeImpl*> discovered;
    pending.push(start);

    while (!pending.empty()) {
        auto node = pending.front();
        pending.pop();
        if (!discovered.insert(node.get()).second) continue;
        nodes_.push_back(node);
        for (auto& neighbor : node->neighbors()) pending.push(std::move(neighbor));
    }

    std::unordered_map<NodeImpl*, std::size_t> indegree;
    for (const auto& node : nodes_) indegree[node.get()] = 0;
    for (const auto& node : nodes_) {
        for (const auto& downstream : node->downstream_nodes()) {
            if (indegree.contains(downstream.get())) ++indegree[downstream.get()];
        }
    }

    std::queue<NodeImpl*> roots;
    for (const auto& [node, degree] : indegree) {
        if (degree == 0) roots.push(node);
    }
    while (!roots.empty()) {
        auto* node = roots.front();
        roots.pop();
        topological_order_.push_back(node);
        for (const auto& downstream : node->downstream_nodes()) {
            auto found = indegree.find(downstream.get());
            if (found != indegree.end() && --found->second == 0) roots.push(found->first);
        }
    }
    if (topological_order_.size() != nodes_.size()) {
        return unexpected(Error{ErrorCode::graph_cycle, "connected topology contains a cycle"});
    }

    for (const auto& node : nodes_) node->retain_pipeline();

    contexts_.reserve(nodes_.size());
    owned_contexts_.reserve(nodes_.size());

    for (const auto& node : nodes_) {
        auto context = std::make_unique<NodeContext>();
        context->node = node.get();
        context->actor = std::make_shared<Actor>(
            /*capacity*/ 1024,
            /*multi_enqueuer*/ true);

        const auto& inputs = node->input_ports();
        context->input_types.reserve(inputs.size());
        for (const auto& port : inputs) {
            context->input_types.push_back(port ? port->type_id() : TypeId{});
        }

        const auto& outputs = node->output_ports();
        context->outputs.resize(outputs.size());
        for (std::size_t i = 0; i < outputs.size(); ++i) {
            const auto& port = outputs[i];
            auto& route = context->outputs[i];
            route.type = port ? port->type_id() : TypeId{};
            if (port && !port->is_connected_any()) {
                route.terminal_queue = std::make_shared<TerminalQueue<AnyPayload>>();
            }
        }

        auto* raw = context.get();
        owned_contexts_.push_back(std::move(context));
        contexts_.emplace(node.get(), raw);
    }

    topological_contexts_.reserve(topological_order_.size());
    for (auto* node : topological_order_) {
        auto* context = context_for(node);
        assert(context && "topological node context missing");
        topological_contexts_.push_back(context);
    }

    for (auto* node : topological_order_) {
        auto* context = context_for(node);
        assert(context && "node context missing");
        context->runtime = node->create_runtime(*this);
    }

    for (const auto& node : nodes_) {
        auto* context = context_for(node.get());
        assert(context && "node context missing");
        const auto& outputs = node->output_ports();
        for (std::size_t i = 0; i < outputs.size(); ++i) {
            const auto& port = outputs[i];
            if (!port) continue;
            if (!port->is_connected_any()) continue;

            auto destination = port->destination_any();
            if (!destination) {
                return unexpected(Error{ErrorCode::invalid_graph, "connected output is missing destination port"});
            }

            auto dest_node = destination->owner_internal();
            if (!dest_node) {
                return unexpected(Error{ErrorCode::invalid_graph, "connected output destination node is unavailable"});
            }
            auto* dest_context = context_for(dest_node.get());
            if (!dest_context) {
                return unexpected(Error{ErrorCode::invalid_graph, "connected output destination context is unavailable"});
            }
            auto actor = actor_for(dest_context);
            if (!actor) {
                return unexpected(Error{ErrorCode::invalid_graph, "connected output destination actor is unavailable"});
            }

            auto& route = context->outputs.at(port->port_index());
            route.connection.emplace(ConnectionState{
                dest_context,
                destination->type_id(),
                destination->port_index(),
                std::move(actor),
                port->connection_options(),
            });
        }
    }

    for (const auto& node : nodes_) {
        auto* context = context_for(node.get());
        assert(context && "node context missing");
        for (const auto& port : node->output_ports()) {
            if (!port) continue;
            auto& route = context->outputs.at(port->port_index());
            if (port->is_connected_any()) {
                if (!route.connection.has_value()) {
                    return unexpected(Error{ErrorCode::invalid_graph, "connection state is missing"});
                }
                continue;
            }
            if (!route.terminal_queue) {
                return unexpected(Error{ErrorCode::invalid_graph, "output queue is missing"});
            }
        }
    }
    return {};
}

PipelineImpl::~PipelineImpl() {
    if (!inputs_finished_.load() && !cancelled_.load()) cancel();
    for (const auto& context : owned_contexts_) {
        if (!context) continue;
        for (auto& route : context->outputs) {
            if (route.connection) route.connection->queue->stop();
        }
    }
    for (const auto& context : owned_contexts_) {
        if (context && context->actor) context->actor->stop();
    }
    for (const auto& node : nodes_) node->release_pipeline();
    if (auto* err = terminal_error_.exchange(nullptr)) {
        delete err;
    }
}

Result<std::shared_ptr<INodeSession>> PipelineImpl::get_node_session(
    const std::shared_ptr<INode>& node) {
    debug_assert_pipeline_thread();
    auto internal_node = std::dynamic_pointer_cast<NodeImpl>(node);
    if (!internal_node) {
        return unexpected(Error{ErrorCode::invalid_argument, "node is not a rebar node"});
    }
    auto found = std::find_if(nodes_.begin(), nodes_.end(), [&](const auto& candidate) {
        return candidate.get() == internal_node.get();
    });
    if (found == nodes_.end()) {
        return unexpected(Error{ErrorCode::node_not_in_pipeline, "node is not part of this pipeline session"});
    }

    if (auto existing = sessions_[internal_node.get()].lock()) return existing;
    auto created = std::make_shared<NodeSessionImpl>(shared_from_this(), internal_node);
    sessions_[internal_node.get()] = created;
    return std::static_pointer_cast<INodeSession>(created);
}

Result<void> PipelineImpl::submit() {
    debug_assert_pipeline_thread();
    auto open = ensure_input_open();
    if (!open) return open;

    std::vector<std::shared_ptr<StagedInput>> inputs;
    auto out = staged_inputs_.begin();
    for (auto it = staged_inputs_.begin(); it != staged_inputs_.end(); ++it) {
        if (auto input = it->lock()) {
            inputs.push_back(std::move(input));
            *out++ = *it;
        }
    }
    staged_inputs_.erase(out, staged_inputs_.end());

    for (const auto& input : inputs) {
        auto result = input->flush_staged();
        if (!result) {
            fail(result.error());
            return result;
        }
    }
    return {};
}

Result<void> PipelineImpl::finish_inputs() {
    debug_assert_pipeline_thread();
    if (cancelled_.load()) {
        return unexpected(Error{ErrorCode::cancelled, "pipeline is cancelled"});
    }
    if (auto* error = terminal_error_.load()) return unexpected(*error);
    if (inputs_finished_.load()) return {};

    if (has_pending_input()) {
        auto submitted = submit();
        if (!submitted) return submitted;
    }

    auto idle = wait_for_idle();
    if (!idle) return idle;

    if (cancelled_.load()) {
        return unexpected(Error{ErrorCode::cancelled, "pipeline is cancelled"});
    }
    if (auto* error = terminal_error_.load()) return unexpected(*error);

    inputs_finished_.store(true);

    for (auto* context : topological_contexts_) {
        if (!context || !context->runtime) continue;
        auto result = context->runtime->finish();
        if (!result) {
            fail(result.error());
            return result;
        }
    }
    for (const auto& context : owned_contexts_) {
        if (!context) continue;
        for (auto& route : context->outputs) {
            if (route.terminal_queue) route.terminal_queue->finish();
        }
    }
    return {};
}

void PipelineImpl::teardown() noexcept {
    fail(Error{ErrorCode::cancelled, "pipeline was torn down"});
    cancel();
    inputs_finished_.store(true);
    staged_inputs_.clear();
    for (const auto& context : owned_contexts_) {
        if (!context) continue;
        for (auto& route : context->outputs) {
            if (route.terminal_queue) route.terminal_queue->cancel();
        }
    }
}

void PipelineImpl::cancel() noexcept {
    cancelled_.store(true);
    for (const auto& context : owned_contexts_) {
        if (context && context->runtime) context->runtime->cancel();
    }
    for (const auto& context : owned_contexts_) {
        if (!context) continue;
        for (auto& route : context->outputs) {
            if (route.connection) route.connection->queue->stop();
        }
    }
    for (const auto& context : owned_contexts_) {
        if (context && context->actor) context->actor->stop();
    }
    for (const auto& context : owned_contexts_) {
        if (!context) continue;
        for (auto& route : context->outputs) {
            if (route.terminal_queue) route.terminal_queue->cancel();
        }
    }
    std::lock_guard lock(pending_mutex_);
    pending_cv_.notify_all();
}

Result<void> PipelineImpl::ensure_input_open() const {
    if (cancelled_.load()) {
        return unexpected(Error{ErrorCode::cancelled, "pipeline is cancelled"});
    }
    if (auto* error = terminal_error_.load()) return unexpected(*error);
    if (inputs_finished_.load()) {
        return unexpected(Error{ErrorCode::input_closed, "pipeline inputs are finished"});
    }
    return {};
}

Result<void> PipelineImpl::deliver_payload(
    NodeContext* node,
    PortId input,
    AnyPayload payload) {
    if (!node) {
        return unexpected(Error{ErrorCode::invalid_argument, "destination node is null"});
    }
    if (!payload) {
        return unexpected(Error{ErrorCode::invalid_argument, "payload is null"});
    }
    if (input >= node->input_types.size()) {
        return unexpected(Error{ErrorCode::port_not_available, "input port is unavailable"});
    }
    if (payload.type_id() != node->input_types[input]) {
        return unexpected(Error{
            ErrorCode::invalid_argument,
            "payload type does not match input port type",
        });
    }
    auto open = ensure_input_open();
    if (!open) return open;
    if (!node->runtime) {
        return unexpected(Error{ErrorCode::node_not_in_pipeline, "node runtime is unavailable"});
    }
    auto result = node->runtime->on_payload(input, std::move(payload));
    if (!result) fail(result.error());
    return result;
}

Result<void> PipelineImpl::forward_payload(
    NodeContext* node,
    PortId output,
    AnyPayload payload,
    bool final) {
    if (!node) {
        return unexpected(Error{ErrorCode::invalid_argument, "source node is null"});
    }
    if (!payload) {
        return unexpected(Error{ErrorCode::invalid_argument, "payload is null"});
    }
    if (output >= node->outputs.size()) {
        return unexpected(Error{ErrorCode::port_not_available, "output port is unavailable"});
    }
    const auto& route = node->outputs[output];
    if (payload.type_id() != route.type) {
        return unexpected(Error{
            ErrorCode::invalid_argument,
            "payload type does not match output type",
        });
    }
    if (route.connection) {
        const auto* connection = &*route.connection;
        if (!connection->destination_actor) {
            return unexpected(Error{ErrorCode::invalid_graph, "destination actor is unavailable"});
        }

        QueuedPayload queued;
        queued.payload = std::move(payload);
        queued.token = acquire_pending_token();

        auto queued_result = connection->queue->push(std::move(queued));
        if (!queued_result) {
            return unexpected(queued_result.error());
        }
        if (*queued_result) {
            if (!connection->destination_actor->post([this, connection] { drain_connection(connection); })) {
                return unexpected(Error{ErrorCode::cancelled, "destination actor is closed"});
            }
        }
        return {};
    }

    if (!route.terminal_queue) {
        return unexpected(Error{ErrorCode::output_closed, "output queue is unavailable"});
    }
    route.terminal_queue->push(OutputItem<AnyPayload>{std::move(payload), final});
    return {};
}

std::shared_ptr<TerminalQueue<AnyPayload>> PipelineImpl::output_queue(
    NodeContext* node,
    PortId output) const {
    if (!node || output >= node->outputs.size()) return nullptr;
    return node->outputs[output].terminal_queue;
}

std::shared_ptr<TerminalQueue<AnyPayload>> PipelineImpl::ensure_output_queue(
    NodeContext* node,
    PortId output) {
    if (!node || output >= node->outputs.size()) return nullptr;
    auto& queue = node->outputs[output].terminal_queue;
    if (!queue) queue = std::make_shared<TerminalQueue<AnyPayload>>();
    return queue;
}

void PipelineImpl::register_staged_input(const std::shared_ptr<StagedInput>& input) {
    debug_assert_pipeline_thread();
    staged_inputs_.push_back(input);
}

bool PipelineImpl::has_pending_input() const {
    for (const auto& weak : staged_inputs_) {
        if (auto input = weak.lock(); input && input->has_pending()) return true;
    }
    return false;
}

void PipelineImpl::fail(const Error& error) {
    if (cancelled_.load()) return;
    Error* expected = nullptr;
    auto* desired = new Error(error);
    if (!terminal_error_.compare_exchange_strong(expected, desired)) {
        delete desired;
        return;
    }
    for (const auto& context : owned_contexts_) {
        if (!context) continue;
        for (auto& route : context->outputs) {
            if (route.connection) route.connection->queue->stop();
        }
    }
    for (const auto& context : owned_contexts_) {
        if (!context) continue;
        for (auto& route : context->outputs) {
            if (route.terminal_queue) route.terminal_queue->fail(error);
        }
    }
    std::lock_guard lock(pending_mutex_);
    pending_cv_.notify_all();
}

Result<void> PipelineImpl::wait_for_idle() {
    std::unique_lock lock(pending_mutex_);
    pending_cv_.wait(lock, [&] {
        return pending_deliveries_.load() == 0 || cancelled_.load() || terminal_error_.load();
    });

    if (cancelled_.load()) {
        return unexpected(Error{ErrorCode::cancelled, "pipeline is cancelled"});
    }
    if (auto* error = terminal_error_.load()) return unexpected(*error);
    return {};
}

void PipelineImpl::drain_connection(const ConnectionState* connection) {
    if (!connection) return;
    QueuedPayload queued;
    while (connection->queue->pop(queued)) {
        auto payload = std::move(queued.payload);
        auto token = std::move(queued.token);
        (void)deliver_payload(
            connection->destination,
            connection->destination_port,
            std::move(payload));
        (void)token;
    }
}

}  // namespace rebar_ml::internal
