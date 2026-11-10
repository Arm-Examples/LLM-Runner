//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "catch2/catch_test_macros.hpp"

#include "core/runtime/builtin_typed_node_runtime.hpp"
#include "core/runtime_internal.hpp"
#include "rebar/media.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using rebar_ml::ConnectionOptions;
using rebar_ml::ErrorCode;
using rebar_ml::OverflowPolicy;
using rebar_ml::TextData;
using rebar_ml::internal::BuiltInTypedNodeRuntime;
using rebar_ml::internal::NodeContext;
using rebar_ml::internal::NodeImpl;
using rebar_ml::internal::NodeRuntime;
using rebar_ml::internal::PipelineImpl;

class ForwardingTextNode;

class ForwardingTextRuntime final : public BuiltInTypedNodeRuntime {
public:
    ForwardingTextRuntime(PipelineImpl& pipeline, ForwardingTextNode& node, NodeContext* context, std::chrono::milliseconds delay)
        : pipeline_(pipeline), node_(node), context_(context), delay_(delay) {}

    rebar_ml::Result<void> on_text(std::size_t input, TextData text) override;

private:
    PipelineImpl& pipeline_;
    ForwardingTextNode& node_;
    NodeContext* context_ = nullptr;
    std::chrono::milliseconds delay_;
};

class ForwardingTextNode final : public NodeImpl {
public:
    explicit ForwardingTextNode(std::string name, std::chrono::milliseconds delay)
        : NodeImpl(std::move(name)), delay_(delay) {
        add_input<TextData>();
        add_output<TextData>();
    }

    std::unique_ptr<NodeRuntime> create_runtime(PipelineImpl& pipeline) override {
        return std::make_unique<ForwardingTextRuntime>(pipeline, *this, pipeline.context_for(this), delay_);
    }

private:
    std::chrono::milliseconds delay_;
};

rebar_ml::Result<void> ForwardingTextRuntime::on_text(std::size_t input, TextData text) {
    if (input != 0) {
        return rebar_ml::unexpected(rebar_ml::Error{
            ErrorCode::port_not_available,
            "text input port is unavailable",
        });
    }
    if (delay_.count() > 0) std::this_thread::sleep_for(delay_);
    return pipeline_.forward<TextData>(context_, 0, std::move(text), true);
}

std::vector<std::string> drain_text_output(const std::shared_ptr<rebar_ml::IOutputStream<TextData>>& out) {
    std::vector<std::string> texts;
    while (true) {
        auto received = out->receive();
        REQUIRE(received.status == rebar_ml::RetrievalStatus::success);
        if (!received.item) break;
        texts.push_back(received.item->data.text);
    }
    return texts;
}

}  // namespace

TEST_CASE("ConnectionOptions overflow=fail returns queue_full", "[rebar_ml][connection_options]") {
    auto source = std::make_shared<ForwardingTextNode>("source", std::chrono::milliseconds{0});
    auto sink = std::make_shared<ForwardingTextNode>("sink", std::chrono::milliseconds{50});

    auto out = source->get_output_port<TextData>(0);
    auto in = sink->get_input_port<TextData>(0);
    REQUIRE(out);
    REQUIRE(in);

    ConnectionOptions options;
    options.queue_capacity = 1;
    options.overflow = OverflowPolicy::fail;
    REQUIRE(out->connect(in, options));

    auto pipeline_session_result = source->create_pipeline_session();
    REQUIRE(pipeline_session_result);
    auto pipeline_session = *pipeline_session_result;

    auto source_session_result = pipeline_session->get_node_session(source);
    REQUIRE(source_session_result);
    auto source_session = *source_session_result;

    auto sink_session_result = pipeline_session->get_node_session(sink);
    REQUIRE(sink_session_result);
    auto sink_session = *sink_session_result;

    auto text_in = source_session->get_input<TextData>();
    auto text_out = sink_session->get_output<TextData>();
    REQUIRE(text_in);
    REQUIRE(text_out);

    for (int i = 0; i < 10; ++i) {
        TextData text;
        text.text = "msg" + std::to_string(i);
        REQUIRE(text_in->send(text));
    }

    auto finished = pipeline_session->finish_inputs();
    REQUIRE_FALSE(finished);
    REQUIRE(finished.error().code == ErrorCode::queue_full);
}

TEST_CASE("ConnectionOptions overflow=block delivers all items", "[rebar_ml][connection_options]") {
    auto source = std::make_shared<ForwardingTextNode>("source", std::chrono::milliseconds{0});
    auto sink = std::make_shared<ForwardingTextNode>("sink", std::chrono::milliseconds{20});

    auto out = source->get_output_port<TextData>(0);
    auto in = sink->get_input_port<TextData>(0);
    REQUIRE(out);
    REQUIRE(in);

    ConnectionOptions options;
    options.queue_capacity = 1;
    options.overflow = OverflowPolicy::block;
    REQUIRE(out->connect(in, options));

    auto pipeline_session_result = source->create_pipeline_session();
    REQUIRE(pipeline_session_result);
    auto pipeline_session = *pipeline_session_result;

    auto source_session_result = pipeline_session->get_node_session(source);
    REQUIRE(source_session_result);
    auto source_session = *source_session_result;

    auto sink_session_result = pipeline_session->get_node_session(sink);
    REQUIRE(sink_session_result);
    auto sink_session = *sink_session_result;

    auto text_in = source_session->get_input<TextData>();
    auto text_out = sink_session->get_output<TextData>();
    REQUIRE(text_in);
    REQUIRE(text_out);

    constexpr int kMessages = 5;
    for (int i = 0; i < kMessages; ++i) {
        TextData text;
        text.text = "msg" + std::to_string(i);
        REQUIRE(text_in->send(text));
    }

    REQUIRE(pipeline_session->finish_inputs());

    auto texts = drain_text_output(text_out);
    REQUIRE(texts.size() == static_cast<std::size_t>(kMessages));
    for (int i = 0; i < kMessages; ++i) {
        REQUIRE(texts[static_cast<std::size_t>(i)] == "msg" + std::to_string(i));
    }
}

TEST_CASE("ConnectionOptions overflow=drop_newest drops items", "[rebar_ml][connection_options]") {
    auto source = std::make_shared<ForwardingTextNode>("source", std::chrono::milliseconds{0});
    auto sink = std::make_shared<ForwardingTextNode>("sink", std::chrono::milliseconds{50});

    auto out = source->get_output_port<TextData>(0);
    auto in = sink->get_input_port<TextData>(0);
    REQUIRE(out);
    REQUIRE(in);

    ConnectionOptions options;
    options.queue_capacity = 1;
    options.overflow = OverflowPolicy::drop_newest;
    REQUIRE(out->connect(in, options));

    auto pipeline_session_result = source->create_pipeline_session();
    REQUIRE(pipeline_session_result);
    auto pipeline_session = *pipeline_session_result;

    auto source_session_result = pipeline_session->get_node_session(source);
    REQUIRE(source_session_result);
    auto source_session = *source_session_result;

    auto sink_session_result = pipeline_session->get_node_session(sink);
    REQUIRE(sink_session_result);
    auto sink_session = *sink_session_result;

    auto text_in = source_session->get_input<TextData>();
    auto text_out = sink_session->get_output<TextData>();
    REQUIRE(text_in);
    REQUIRE(text_out);

    constexpr int kMessages = 10;
    for (int i = 0; i < kMessages; ++i) {
        TextData text;
        text.text = "msg" + std::to_string(i);
        REQUIRE(text_in->send(text));
    }

    REQUIRE(pipeline_session->finish_inputs());

    auto texts = drain_text_output(text_out);
    REQUIRE_FALSE(texts.empty());
    REQUIRE(texts.size() < static_cast<std::size_t>(kMessages));
}

TEST_CASE("ConnectionOptions overflow=drop_oldest keeps newest items", "[rebar_ml][connection_options]") {
    auto source = std::make_shared<ForwardingTextNode>("source", std::chrono::milliseconds{0});
    auto sink = std::make_shared<ForwardingTextNode>("sink", std::chrono::milliseconds{50});

    auto out = source->get_output_port<TextData>(0);
    auto in = sink->get_input_port<TextData>(0);
    REQUIRE(out);
    REQUIRE(in);

    ConnectionOptions options;
    options.queue_capacity = 1;
    options.overflow = OverflowPolicy::drop_oldest;
    REQUIRE(out->connect(in, options));

    auto pipeline_session_result = source->create_pipeline_session();
    REQUIRE(pipeline_session_result);
    auto pipeline_session = *pipeline_session_result;

    auto source_session_result = pipeline_session->get_node_session(source);
    REQUIRE(source_session_result);
    auto source_session = *source_session_result;

    auto sink_session_result = pipeline_session->get_node_session(sink);
    REQUIRE(sink_session_result);
    auto sink_session = *sink_session_result;

    auto text_in = source_session->get_input<TextData>();
    auto text_out = sink_session->get_output<TextData>();
    REQUIRE(text_in);
    REQUIRE(text_out);

    constexpr int kMessages = 10;
    for (int i = 0; i < kMessages; ++i) {
        TextData text;
        text.text = "msg" + std::to_string(i);
        REQUIRE(text_in->send(text));
    }

    REQUIRE(pipeline_session->finish_inputs());

    auto texts = drain_text_output(text_out);
    REQUIRE_FALSE(texts.empty());
    REQUIRE(texts.back() == "msg" + std::to_string(kMessages - 1));
}
