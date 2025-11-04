//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//


#include "LlmImpl.hpp"
#include <cstring>
#include <nlohmann/json.hpp>
#include "catch2/catch_test_macros.hpp"

using json = nlohmann::json;

static std::string buildVariant(const std::string& baseConfig, const std::function<void(json&)>& mutator)
{
    json j = json::parse(baseConfig);
    mutator(j);
    return j.dump(4);
}

/**
 * Simple Test file for testing config related cases
 */
TEST_CASE("Configuration paramaters test")
{
    std::string validConfig =
        R"JSON(
        {
            "chat" : {
                "systemPrompt": "You are a helpful and factual AI assistant named Orbita. Orbita answers with maximum of two sentences.",
                "applyDefaultChatTemplate": false,
                "systemTemplate" : "<|system|>%s<|end|>",
                "userTemplate"   : "<|user|>%s<|end|><|assistant|>"
            },
            "model" : {
                "llmModelName" : "llama.cpp/phi-2/model.gguf",
                "isVision" : false
            },
            "runtime" : {
                "batchSize" : 256,
                "numThreads" : 4,
                "contextSize" : 2048
            },
            "stopWords": ["endoftext"]
            }
        )JSON";

    SECTION("Set parameter") {
        LlmConfig config(validConfig);
        int newNumThreads = 8;
        config.SetConfigInt("numThreads",newNumThreads);
        CHECK(newNumThreads == config.GetConfigInt("numThreads"));
    }

    SECTION("Bad chat parameters") {
        try {
            // Wrong type: chat.systemPrompt -> bool
            const std::string badChatParamConfig = buildVariant(validConfig, [](json& j) {
                j["chat"]["systemPrompt"] = false;
            });

            LlmConfig config(badChatParamConfig);
            CHECK(false);
        } catch (const std::invalid_argument& e) {
            CHECK(std::string(e.what()).find("config: schema/type error") != std::string::npos);
        }
    }

    SECTION("Missing runtime parameters") {
        try {
            // Missing param: remove top-level "runtime"
            const std::string missingParamConfig = buildVariant(validConfig, [](json& j) {
                j.erase("runtime");
            });

            LlmConfig config(missingParamConfig);
        } catch (const nlohmann::json::out_of_range& e) {
            CHECK(std::string(e.what()).find("'runtime' not found") != std::string::npos);
        }
    }

    SECTION("Missing sub chat parameter") {
        try {
              // Missing sub-param: remove "chat.userTemplate"
            const std::string missingSubParamConfig = buildVariant(validConfig, [](json& j) {
                if (j.contains("chat")) j["chat"].erase("userTemplate");
            });

            LlmConfig config(missingSubParamConfig);
            CHECK(false);
        } catch (const std::invalid_argument& e) {
            CHECK(std::string(e.what()).find("config: schema/type error") != std::string::npos);
        }
    }
}
