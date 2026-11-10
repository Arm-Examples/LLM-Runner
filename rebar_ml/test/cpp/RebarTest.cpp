//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "rebar/tee.hpp"

#include "catch2/catch_test_macros.hpp"

#include <memory>
#include <string>

TEST_CASE("rebar tests can locate root binaries", "[rebar]")
{
    REQUIRE_FALSE(std::string(REBAR_ROOT_BIN_DIR).empty());
    REQUIRE_FALSE(std::string(REBAR_ROOT_LIB_DIR).empty());
}

TEST_CASE("Text tee duplicates to all outputs", "[rebar]")
{
    auto tee = rebar_ml::make_tee_node(
        rebar_ml::TeeOptions{.output_count = 2});

    auto pipeline_session_result = tee->create_pipeline_session();
    REQUIRE(pipeline_session_result);
    auto pipeline_session = *pipeline_session_result;

    auto node_session_result = pipeline_session->get_node_session(tee);
    REQUIRE(node_session_result);
    auto session = *node_session_result;

    auto text_in = session->get_input<rebar_ml::TextData>();
    auto out0 = session->get_output<rebar_ml::TextData>(0);
    auto out1 = session->get_output<rebar_ml::TextData>(1);
    REQUIRE(text_in);
    REQUIRE(out0);
    REQUIRE(out1);

    rebar_ml::TextData text;
    text.text = "hello";
    text.language = std::string("en");

    REQUIRE(text_in->send(text));
    REQUIRE(pipeline_session->finish_inputs());

    auto r0 = out0->receive();
    REQUIRE(r0.status == rebar_ml::RetrievalStatus::success);
    REQUIRE(r0.item.has_value());
    REQUIRE(r0.item->data.text == "hello");

    auto r1 = out1->receive();
    REQUIRE(r1.status == rebar_ml::RetrievalStatus::success);
    REQUIRE(r1.item.has_value());
    REQUIRE(r1.item->data.text == "hello");

    auto end0 = out0->receive();
    REQUIRE(end0.status == rebar_ml::RetrievalStatus::success);
    REQUIRE_FALSE(end0.item.has_value());

    auto end1 = out1->receive();
    REQUIRE(end1.status == rebar_ml::RetrievalStatus::success);
    REQUIRE_FALSE(end1.item.has_value());
}
