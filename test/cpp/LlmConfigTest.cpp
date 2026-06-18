//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmConfig.hpp"

#include "catch2/catch_test_macros.hpp"

#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

std::string ValidConfig()
{
    return R"JSON(
    {
        "chat": {
            "systemPrompt": "You are a helpful assistant.",
            "applyDefaultChatTemplate": false,
            "systemTemplate": "<|system|>%s<|end|>",
            "userTemplate": "<|user|>%s<|end|><|assistant|>"
        },
        "model": {
            "llmModelName": "llama.cpp/phi-2/model.gguf",
            "isVision": false
        },
        "runtime": {
            "batchSize": 256,
            "numThreads": 4,
            "contextSize": 2048
        },
        "stopWords": ["endoftext"]
    }
    )JSON";
}

std::string BuildVariant(const std::function<void(json&)>& mutator)
{
    json j = json::parse(ValidConfig());
    mutator(j);
    return j.dump(4);
}

bool Contains(const std::string& text, const std::string& expected)
{
    return text.find(expected) != std::string::npos;
}

} // namespace

TEST_CASE("LlmConfig: valid config parses all sections")
{
    LlmConfig config(ValidConfig());

    CHECK(config.GetChat().systemPrompt == "You are a helpful assistant.");
    CHECK(config.GetChat().applyDefaultChatTemplate == false);
    CHECK(config.GetChat().systemTemplate == "<|system|>%s<|end|>");
    CHECK(config.GetChat().userTemplate == "<|user|>%s<|end|><|assistant|>");

    CHECK(config.GetModel().llmModelName == "llama.cpp/phi-2/model.gguf");
    CHECK(config.GetModel().isVision == false);
    CHECK(config.GetModel().projModelName.empty());
    CHECK(config.GetModel().maxInputDimension == 128);

    CHECK(config.GetRuntime().batchSize == 256);
    CHECK(config.GetRuntime().numThreads == 4);
    CHECK(config.GetRuntime().contextSize == 2048);

    REQUIRE(config.GetStopWords().size() == 1);
    CHECK(config.GetStopWords()[0] == "endoftext");
}

TEST_CASE("LlmConfig: optional model fields can be configured")
{
    const std::string configJson = BuildVariant([](json& j) {
        j["model"]["projModelName"] = "projection.gguf";
        j["model"]["maxInputDimension"] = 256;
        j["model"]["isVision"] = true;
    });

    LlmConfig config(configJson);

    CHECK(config.GetConfigString(LlmConfig::ConfigParam::ProjModelName) == "projection.gguf");
    CHECK(config.GetConfigInt(LlmConfig::ConfigParam::MaxInputDimension) == 256);
    CHECK(config.GetConfigBool(LlmConfig::ConfigParam::IsVision) == true);
}

TEST_CASE("LlmConfig: string getters and setters work for string parameters")
{
    LlmConfig config(ValidConfig());

    config.SetConfigString(LlmConfig::ConfigParam::SystemPrompt, "new system prompt");
    config.SetConfigString(LlmConfig::ConfigParam::SystemTemplate, "SYS:%s");
    config.SetConfigString(LlmConfig::ConfigParam::UserTemplate, "USR:%s");
    config.SetConfigString(LlmConfig::ConfigParam::LlmModelName, "new-model.gguf");
    config.SetConfigString(LlmConfig::ConfigParam::ProjModelName, "new-proj.gguf");

    CHECK(config.GetConfigString(LlmConfig::ConfigParam::SystemPrompt) == "new system prompt");
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::SystemTemplate) == "SYS:%s");
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::UserTemplate) == "USR:%s");
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::LlmModelName) == "new-model.gguf");
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::ProjModelName) == "new-proj.gguf");
}

TEST_CASE("LlmConfig: bool getters and setters work for bool parameters")
{
    LlmConfig config(ValidConfig());

    config.SetConfigBool(LlmConfig::ConfigParam::ApplyDefaultChatTemplate, true);
    config.SetConfigBool(LlmConfig::ConfigParam::IsVision, true);

    CHECK(config.GetConfigBool(LlmConfig::ConfigParam::ApplyDefaultChatTemplate) == true);
    CHECK(config.GetConfigBool(LlmConfig::ConfigParam::IsVision) == true);

    config.SetConfigBool(LlmConfig::ConfigParam::ApplyDefaultChatTemplate, false);
    config.SetConfigBool(LlmConfig::ConfigParam::IsVision, false);

    CHECK(config.GetConfigBool(LlmConfig::ConfigParam::ApplyDefaultChatTemplate) == false);
    CHECK(config.GetConfigBool(LlmConfig::ConfigParam::IsVision) == false);
}

TEST_CASE("LlmConfig: int getters and setters work for int parameters")
{
    LlmConfig config(ValidConfig());

    config.SetConfigInt(LlmConfig::ConfigParam::NumThreads, 8);
    config.SetConfigInt(LlmConfig::ConfigParam::BatchSize, 512);
    config.SetConfigInt(LlmConfig::ConfigParam::ContextSize, 4096);
    config.SetConfigInt(LlmConfig::ConfigParam::MaxInputDimension, 320);

    CHECK(config.GetConfigInt(LlmConfig::ConfigParam::NumThreads) == 8);
    CHECK(config.GetConfigInt(LlmConfig::ConfigParam::BatchSize) == 512);
    CHECK(config.GetConfigInt(LlmConfig::ConfigParam::ContextSize) == 4096);
    CHECK(config.GetConfigInt(LlmConfig::ConfigParam::MaxInputDimension) == 320);
}

TEST_CASE("LlmConfig: SetStopWords updates stop words")
{
    LlmConfig config(ValidConfig());

    config.SetStopWords({"</s>", "<|end|>"});

    REQUIRE(config.GetStopWords().size() == 2);
    CHECK(config.GetStopWords()[0] == "</s>");
    CHECK(config.GetStopWords()[1] == "<|end|>");
}

TEST_CASE("LlmConfig: invalid JSON throws invalid_argument")
{
    CHECK_THROWS_AS(LlmConfig("{ invalid json }"), std::invalid_argument);
}

TEST_CASE("LlmConfig: missing required top-level sections throw")
{
    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j.erase("chat");
    })), nlohmann::json::out_of_range);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j.erase("model");
    })), nlohmann::json::out_of_range);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j.erase("runtime");
    })), nlohmann::json::out_of_range);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j.erase("stopWords");
    })), nlohmann::json::out_of_range);
}

TEST_CASE("LlmConfig: missing required nested fields throw schema error")
{
    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["chat"].erase("userTemplate");
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["runtime"].erase("numThreads");
    })), std::invalid_argument);
}

TEST_CASE("LlmConfig: missing model llmModelName uses default value")
{
    LlmConfig config(BuildVariant([](json& j) {
        j["model"].erase("llmModelName");
    }));

    CHECK(config.GetConfigString(LlmConfig::ConfigParam::LlmModelName).empty());
}

TEST_CASE("LlmConfig: wrong field types throw schema error")
{
    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["chat"]["systemPrompt"] = false;
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["runtime"]["batchSize"] = "256";
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["model"]["isVision"] = "false";
    })), std::invalid_argument);
}

TEST_CASE("LlmConfig: runtime integer values must be positive")
{
    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["runtime"]["numThreads"] = 0;
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["runtime"]["batchSize"] = 0;
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["runtime"]["contextSize"] = 0;
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["model"]["maxInputDimension"] = 0;
    })), std::invalid_argument);
}

TEST_CASE("LlmConfig: stopWords must be a non-empty array of non-empty strings")
{
    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["stopWords"] = json::array();
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["stopWords"] = "endoftext";
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["stopWords"] = json::array({"valid", 123});
    })), std::invalid_argument);

    CHECK_THROWS_AS(LlmConfig(BuildVariant([](json& j) {
        j["stopWords"] = json::array({"valid", ""});
    })), std::invalid_argument);
}

TEST_CASE("LlmConfig: SetStopWords rejects invalid values")
{
    LlmConfig config(ValidConfig());

    CHECK_THROWS_AS(config.SetStopWords({}), std::invalid_argument);
    CHECK_THROWS_AS(config.SetStopWords({"valid", ""}), std::invalid_argument);
}

TEST_CASE("LlmConfig: setters reject parameters of the wrong type")
{
    LlmConfig config(ValidConfig());

    CHECK_THROWS_AS(
        config.SetConfigString(LlmConfig::ConfigParam::NumThreads, "bad"),
        std::invalid_argument);

    CHECK_THROWS_AS(
        config.SetConfigBool(LlmConfig::ConfigParam::NumThreads, true),
        std::invalid_argument);

    CHECK_THROWS_AS(
        config.SetConfigInt(LlmConfig::ConfigParam::SystemPrompt, 1),
        std::invalid_argument);
}

TEST_CASE("LlmConfig: getters reject parameters of the wrong type")
{
    LlmConfig config(ValidConfig());

    CHECK_THROWS_AS(
        config.GetConfigString(LlmConfig::ConfigParam::NumThreads),
        std::invalid_argument);

    CHECK_THROWS_AS(
        config.GetConfigBool(LlmConfig::ConfigParam::NumThreads),
        std::invalid_argument);

    CHECK_THROWS_AS(
        config.GetConfigInt(LlmConfig::ConfigParam::SystemPrompt),
        std::invalid_argument);
}

TEST_CASE("LlmConfig: SetConfigInt rejects non-positive values")
{
    LlmConfig config(ValidConfig());

    CHECK_THROWS_AS(config.SetConfigInt(LlmConfig::ConfigParam::NumThreads, 0), std::invalid_argument);
    CHECK_THROWS_AS(config.SetConfigInt(LlmConfig::ConfigParam::BatchSize, 0), std::invalid_argument);
    CHECK_THROWS_AS(config.SetConfigInt(LlmConfig::ConfigParam::ContextSize, 0), std::invalid_argument);
    CHECK_THROWS_AS(config.SetConfigInt(LlmConfig::ConfigParam::MaxInputDimension, 0), std::invalid_argument);
}

TEST_CASE("LlmConfig: thrown messages contain useful context")
{
    try {
        LlmConfig config(BuildVariant([](json& j) {
            j["model"]["maxInputDimension"] = 0;
        }));

        CHECK(false);
    } catch (const std::invalid_argument& e) {
        CHECK(Contains(e.what(), "config.model.maxInputDimension must be positive"));
    }

    try {
        LlmConfig config(BuildVariant([](json& j) {
            j["stopWords"] = json::array({"valid", ""});
        }));

        CHECK(false);
    } catch (const std::invalid_argument& e) {
        CHECK(Contains(e.what(), "config.stopWords: strings must be non-empty"));
    }
}

TEST_CASE("LlmConfig: multi-parameter modification workflow")
{
    LlmConfig config(ValidConfig());

    config.SetConfigInt(LlmConfig::ConfigParam::NumThreads, 16);
    config.SetConfigInt(LlmConfig::ConfigParam::BatchSize, 512);
    config.SetConfigString(LlmConfig::ConfigParam::SystemPrompt, "Updated system prompt");
    config.SetConfigBool(LlmConfig::ConfigParam::IsVision, true);

    CHECK(config.GetConfigInt(LlmConfig::ConfigParam::NumThreads) == 16);
    CHECK(config.GetConfigInt(LlmConfig::ConfigParam::BatchSize) == 512);
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::SystemPrompt) == "Updated system prompt");
    CHECK(config.GetConfigBool(LlmConfig::ConfigParam::IsVision) == true);

    CHECK(config.GetRuntime().contextSize == 2048);
    CHECK(config.GetChat().userTemplate == "<|user|>%s<|end|><|assistant|>");
}

TEST_CASE("LlmConfig: optional projModelName can be set, cleared, and retrieved")
{
    LlmConfig config(ValidConfig());

    CHECK(config.GetModel().projModelName.empty());
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::ProjModelName).empty());

    config.SetConfigString(LlmConfig::ConfigParam::ProjModelName, "vision-proj.gguf");

    CHECK(config.GetModel().projModelName == "vision-proj.gguf");
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::ProjModelName) == "vision-proj.gguf");

    config.SetConfigString(LlmConfig::ConfigParam::ProjModelName, "");

    CHECK(config.GetModel().projModelName.empty());
    CHECK(config.GetConfigString(LlmConfig::ConfigParam::ProjModelName).empty());
}