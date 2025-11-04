//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//


#include "catch2/catch_test_macros.hpp"
#include "LlmConfig.hpp"
#include "LlmImpl.hpp"
#include <sstream>
#include <list>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

extern std::string s_configFilePath;

std::string SetupConfigString() {
    std::string configFilePath = s_configFilePath;
    std::ifstream configFile(configFilePath);
    std::stringstream buffer;
    buffer << configFile.rdbuf(); // Read file into stringstream
    std::string jsonContent = buffer.str();
    return jsonContent;
}

bool contains(const std::string& str, const std::string& sub) {
    return str.find(sub) != std::string::npos;
}

/**
 * Simple Test file for testing config related cases
 */
TEST_CASE("Test logging issues") {

    auto jsonString = SetupConfigString();
    nlohmann::json modelConfig;
    modelConfig = nlohmann::json::parse(jsonString);
    std::string llmModelName = modelConfig["model"]["llmModelName"];

    SECTION("Wrong model path") {
        try {
            LlmConfig configTest(jsonString);
            LLM llm{};
            llm.LlmInit(configTest);
        } catch (std::runtime_error e) {
            CHECK(contains(e.what(),"initialization failed"));
        }
    }

    SECTION("Set Empty Stop Words") {
        try {
            LlmConfig configTest(jsonString);
            configTest.SetStopWords({});
            LLM llm{};
            llm.LlmInit(configTest);
        } catch (std::invalid_argument e) {
            CHECK(contains(e.what(),"config.stopWords: strings must be non-empty"));
        }
    }

    SECTION("Set non-string Stop Words") {
        try {
            modelConfig["stopWords"] = {34,45};
            std::string updatedJsonString = modelConfig.dump();

            LlmConfig configTest(updatedJsonString);

        } catch (std::invalid_argument e) {
            CHECK(contains(e.what(),"config.stopWords: all entries must be strings"));
        }
    }

    SECTION("Set zero threads and batch-size") {
        std::unordered_map<std::string,std::string>  testDictionary = {
            {"batchSize" ,"batch-size"},
             {"numThreads", "number of threads"}
        };
        for (auto& param :{"batchSize" ,"numThreads" }) {
            try {
                modelConfig[param] = 0;
                std::string updatedJsonString = modelConfig.dump();
                LlmConfig configTest(updatedJsonString);
            }
            catch (std::invalid_argument e) {
                CHECK(contains(e.what(),testDictionary[param]+" must be a positive integer."));
            }
        }
    }
}
