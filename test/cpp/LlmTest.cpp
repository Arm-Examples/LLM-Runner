//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include "LlmImpl.hpp"
#include "LlmUtils.hpp"

#include <list>
#include <sstream>

// Function to create the configuration file from CONFIG_FILE_PATH
void SetupTestConfig(std::stringstream& stopWordsStream,
                     LlmConfig* configTest,
                     std::list<std::string>& STOP_WORDS)
{
    std::string configFilePath     = CONFIG_FILE_PATH;
    std::string userConfigFilePath = USER_CONFIG_FILE_PATH;
    auto config                    = Llm::Test::Utils::LoadConfig(configFilePath);
    stopWordsStream.str("");
    stopWordsStream.clear();
    STOP_WORDS.clear();
    stopWordsStream << config["stopWords"];
    std::string word;
    while (std::getline(stopWordsStream, word, ',')) {
        STOP_WORDS.push_back(word);
    }
    std::string testModelsDir = TEST_MODELS_DIR;
    std::string modelPath     = testModelsDir + "/" + config["llmModelName"];
    config["modelPath"]       = modelPath;
    auto userConfig           = Llm::Test::Utils::LoadUserConfig(userConfigFilePath);
    *configTest               = Llm::Test::Utils::GetConfig(config, userConfig);
    configTest->SetModelPath(modelPath);
}
/**
 * Simple query->response test
 * ToDo Replace with more sophisticated context tests if/when reset context is available in Cpp
 * layer
 */
TEST_CASE("Test Llm-Wrapper class")
{
    LlmConfig configTest{};
    std::stringstream stopWordsStream;
    std::list<std::string> STOP_WORDS;
    SetupTestConfig(stopWordsStream, &configTest, STOP_WORDS);

    std::string response;
    std::string question         = "What is the capital of France?" + configTest.GetModelTag();
    std::string prefixedQuestion = configTest.GetLlmPrefix() + question;
    LLM llm;

    SECTION("Simple Query Response")
    {
        llm.LlmInit(configTest);
        llm.Encode(prefixedQuestion);
        bool stop = false;

        while (llm.GetChatProgress() < 100) {
            std::string s = llm.NextToken();
            for (auto& stopWord : STOP_WORDS) {
                if (s.find(stopWord) != std::string::npos) {
                    stop = true;
                    break;
                }
            }
            if (stop)
            {
                break;
            }
            response += s;
        }
        CHECK(response.find("Paris") != std::string::npos);
    }

    /**
     * Test Load Empty Model returns nullptr
     */
    SECTION("Test Load Empty Model")
    {
        std::string emptyString;
        configTest.SetModelPath(emptyString);
        REQUIRE_THROWS(llm.LlmInit(configTest));
    }

    llm.FreeLlm();
}
