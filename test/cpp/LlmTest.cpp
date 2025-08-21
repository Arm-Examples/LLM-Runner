//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#define CATCH_CONFIG_MAIN

#include "catch.hpp"

#include "LlmImpl.hpp"
#include <sstream>
#include <list>

// Function to create the configuration file from CONFIG_FILE_PATH
void SetupTestConfig(LlmConfig *configTest)
{
    std::string configFilePath     = CONFIG_FILE_PATH;
    std::ifstream configFile(configFilePath);
    std::stringstream buffer;
    buffer << configFile.rdbuf();  // Read file into stringstream
    std::string jsonContent = buffer.str();
    *configTest = LlmConfig(jsonContent);
    std::string testModelsDir = TEST_MODELS_DIR;
    std::string modelPath     = testModelsDir + "/" + configTest->GetModelPath();
    configTest->SetModelPath(modelPath);
    // Multimodal only
    if (configTest->GetInputModalities().size() == 2) {
        std::string projModelPath =  testModelsDir + "/" + configTest->GetMMPROJModelPath();
        configTest->SetMMPROJModelPath(projModelPath);
    }
}

/**
 * Simple query->response test
 * ToDo Replace with more sophisticated context tests if/when reset context is available in Cpp
sh */
TEST_CASE("Test Llm-Wrapper class")
{
    LlmConfig configTest{};
    SetupTestConfig(&configTest);
    LLM llm(configTest);
    std::stringstream stopWordsStream;
    std::list<std::string> stopWords;
    std::string question         = "What is the capital of France?" ;

    // Multimodal tests only
    if (configTest.GetInputModalities().size() == 2)
    {
        /**
         * Validate the vision path can describe objects in images.
         */
        SECTION("Describe Image")
        {
            llm.LlmInit(configTest);
            struct Case {
                const char* file;
                const char* expect;
            };

            const std::array<Case, 3> cases{{{"cat.bmp",   "cat"},
                                             {"tiger.bmp", "tiger"},
                                             {"dog.bmp",   "dog"}}};
            bool isFirst = true;
            for (const auto& c : cases) {
                std::string prompt = "Can you describe this image briefly?";
                LLM::EncodePayload payload{prompt, std::string{TEST_RESOURCE_DIR} + "/" + c.file, isFirst};
                std::string response;
                llm.Encode(payload);

                while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (s==llm.endToken)
                        break;
                    response += s;
                }
                CHECK(response.find(c.expect) != std::string::npos);
                isFirst = false;
            }

            llm.FreeLlm();
        }

            /**
             * Validate multi-turn context handling for a follow-up question after an image turn.
             */
        SECTION("Follow Up Question")
        {
            llm.LlmInit(configTest);
            std::string prompt = "What type of dress can you see in this image?";

            LLM::EncodePayload payload{prompt, std::string{TEST_RESOURCE_DIR} + "/" + "kimono.bmp", true};
            llm.Encode(payload);
            std::string response1;
            while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (s==llm.endToken)
                        break;
                    response1 += s;
                }
            CHECK(response1.find("kimono") != std::string::npos);

            payload.textPrompt = "Which country does that dress belong to?";
            payload.isFirstMessage = false;
            payload.imagePath = "";
            std::string response2;

            llm.Encode(payload);
            while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (s==llm.endToken)
                        break;
                    response2 += s;
                }
            CHECK(response2.find("Japan") != std::string::npos);
            llm.FreeLlm();
        }

            /**
             * Validate reset context.
             */

        SECTION("Reset Context")
        {
            llm.LlmInit(configTest);
            std::string prompt =  "Can you describe this image?";

            LLM::EncodePayload payload{prompt, std::string{TEST_RESOURCE_DIR} + "/" + "tiger.bmp", true};
            llm.Encode(payload);

            std::string response;
            while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (s==llm.endToken)
                        break;
                    response += s;
                }
            CHECK(response.find("tiger") != std::string::npos);
            llm.ResetContext();

            // Follow up question after context reset
            prompt = "Tell me more about this image?";
            std::string response2;
            payload.textPrompt = prompt;
            payload.imagePath = "";
            payload.isFirstMessage = false;
            llm.Encode(payload);

            while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (s==llm.endToken)
                        break;
                    response2 += s;
                }
            CHECK(response2.find("tiger") == std::string::npos);
            llm.FreeLlm();
        }
    }

    SECTION("Simple Query Response")
    {
        std::string response;
        llm.LlmInit(configTest);
        LLM::EncodePayload payload{question, "", true};
        llm.Encode(payload);
        while (llm.GetChatProgress() < 100) {
            std::string s = llm.NextToken();
            if (s==llm.endToken)
                break;
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
