//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmBridge.hpp"
#include "catch2/catch_test_macros.hpp"
#include <memory>

TEST_CASE("LlmBridge: add, find, and remove work item")
{
    constexpr long operationId = 424242;

    auto state = std::make_shared<WorkState>();
    state->operationId = operationId;
    state->cancelled = false;

    addWork(state);

    CHECK(findWork(operationId) == state);

    auto removed = removeWork(operationId);

    CHECK(removed == state);
    CHECK(findWork(operationId) == nullptr);
}

TEST_CASE("LlmBridge: find and remove missing work item return null")
{
    constexpr long operationId = -1;

    CHECK(findWork(operationId) == nullptr);
    CHECK(removeWork(operationId) == nullptr);
}

TEST_CASE("LlmBridge: adding same operation id replaces previous work item")
{
    constexpr long operationId = 5050505050L;

    auto first = std::make_shared<WorkState>();
    first->operationId = operationId;
    first->cancelled = false;

    auto second = std::make_shared<WorkState>();
    second->operationId = operationId;
    second->cancelled = true;

    addWork(first);
    addWork(second);

    auto found = findWork(operationId);

    REQUIRE(found != nullptr);
    CHECK(found == second);
    CHECK(found != first);
    CHECK(found->cancelled.load() == true);

    auto removed = removeWork(operationId);

    CHECK(removed == second);
    CHECK(findWork(operationId) == nullptr);
}

TEST_CASE("LlmBridge: cancelled state is preserved")
{
    constexpr long operationId = 9594939291L;

    auto state = std::make_shared<WorkState>();
    state->operationId = operationId;
    state->cancelled = true;

    addWork(state);

    auto found = findWork(operationId);

    REQUIRE(found != nullptr);
    CHECK(found == state);
    CHECK(found->cancelled.load() == true);

    auto removed = removeWork(operationId);

    REQUIRE(removed != nullptr);
    CHECK(removed == state);
    CHECK(removed->cancelled.load() == true);
    CHECK(findWork(operationId) == nullptr);
}