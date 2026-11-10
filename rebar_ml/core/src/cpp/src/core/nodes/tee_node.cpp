//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "rebar/tee.hpp"

#include "core/runtime_internal.hpp"
#include "core/runtime/builtin_typed_node_runtime.hpp"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace rebar_ml {
namespace {

using internal::NodeImpl;
using internal::NodeContext;
using internal::NodeRuntime;
using internal::BuiltInTypedNodeRuntime;
using internal::PipelineImpl;
using rebar_ml::PortId;

template <typename T>
Result<void> forward_to_all_outputs(
    PipelineImpl& pipeline,
    NodeContext* context,
    NodeImpl& node,
    std::size_t output_count,
    T value) {
    if (output_count == 0) return {};

    std::vector<PortId> outputs;
    outputs.reserve(output_count);

    // Prefer terminal/unconnected outputs first so external observers see data even if a connected
    // downstream blocks while processing.
    for (std::size_t i = 0; i < output_count; ++i) {
        auto port = node.template get_output_port<T>(i);
        if (port && !port->is_connected()) outputs.push_back(port->get_index());
    }
    for (std::size_t i = 0; i < output_count; ++i) {
        auto port = node.template get_output_port<T>(i);
        if (port && port->is_connected()) outputs.push_back(port->get_index());
    }

    for (std::size_t idx = 0; idx < outputs.size(); ++idx) {
        const bool last = (idx + 1) == outputs.size();
        auto forwarded = pipeline.forward<T>(
            context, outputs[idx], last ? std::move(value) : value, /*final*/ true);
        if (!forwarded) return forwarded;
    }
    return {};
}

class TeeNode;

class TeeRuntime final : public BuiltInTypedNodeRuntime {
public:
    TeeRuntime(PipelineImpl& pipeline, TeeNode& node, NodeContext* context)
        : pipeline_(pipeline), node_(node), context_(context) {}

    Result<void> on_text(std::size_t input, TextData text) override;
    Result<void> on_audio(std::size_t input, AudioData audio) override;
    Result<void> on_image(std::size_t input, ImageData image) override;

private:
    PipelineImpl& pipeline_;
    TeeNode& node_;
    NodeContext* context_ = nullptr;
};

class TeeNode final : public NodeImpl {
public:
    explicit TeeNode(TeeOptions options)
        : NodeImpl("tee"), options_(std::move(options)) {
        add_input<TextData>();
        add_input<AudioData>();
        add_input<ImageData>();

        if (options_.output_count == 0) options_.output_count = 1;

        for (std::size_t i = 0; i < options_.output_count; ++i) {
            add_output<TextData>();
            add_output<AudioData>();
            add_output<ImageData>();
        }
    }

    std::unique_ptr<NodeRuntime> create_runtime(PipelineImpl& pipeline) override {
        return std::make_unique<TeeRuntime>(pipeline, *this, pipeline.context_for(this));
    }

    std::size_t output_count() const noexcept { return options_.output_count; }

private:
    TeeOptions options_;
};

Result<void> TeeRuntime::on_text(std::size_t input, TextData text) {
    if (input != 0) {
        return unexpected(Error{
            ErrorCode::port_not_available,
            "Tee text input port is unavailable",
        });
    }
    return forward_to_all_outputs<TextData>(
        pipeline_,
        context_,
        node_,
        node_.output_count(),
        std::move(text));
}

Result<void> TeeRuntime::on_audio(std::size_t input, AudioData audio) {
    if (input != 1) {
        return unexpected(Error{
            ErrorCode::port_not_available,
            "Tee audio input port is unavailable",
        });
    }
    return forward_to_all_outputs<AudioData>(
        pipeline_,
        context_,
        node_,
        node_.output_count(),
        std::move(audio));
}

Result<void> TeeRuntime::on_image(std::size_t input, ImageData image) {
    if (input != 2) {
        return unexpected(Error{
            ErrorCode::port_not_available,
            "Tee image input port is unavailable",
        });
    }
    return forward_to_all_outputs<ImageData>(
        pipeline_,
        context_,
        node_,
        node_.output_count(),
        std::move(image));
}

}  // namespace

std::shared_ptr<INode> make_tee_node(TeeOptions options) {
    return std::make_shared<TeeNode>(std::move(options));
}

}  // namespace rebar_ml
