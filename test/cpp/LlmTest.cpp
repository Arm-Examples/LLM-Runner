//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#define CATCH_CONFIG_MAIN


#include "LlmImpl.hpp"
#include "Logger.hpp"
#include <sstream>
#include <list>
#include <iostream>
#include <fstream>
#include <array>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_session.hpp"


#if defined(DEPRECATED)
#undef DEPRECATED
#endif /* defined(DEPRECATED) */


std::string s_configFilePath{""};
std::string s_modelRootDir{""};
std::string s_backendSharedLibraryDir{""};

static int maxTokenRetrievalAttempts = 10000;

using namespace Catch::Clara;

int main(int argc, char* argv[])
{
    Catch::Session session;

    std::string configFilePath;
    std::string modelsRootDir;
    std::string backendSharedLibraryDir;

    auto cli = session.cli() |
                Opt(configFilePath, "configFile")
                ["--config"]
                ("Config (json) file path") |
                Opt(modelsRootDir, "modelRootDir")
                ["--model-root"]
                ("Root directory to look for models") |
                Opt(backendSharedLibraryDir, "sharedLibraryDir")
                ["--backend-shared-lib-dir"]
                        ("Backend shared Library directory");

    session.cli(cli);

    if (0 != session.applyCommandLine(argc, argv)) {
        LOG_ERROR("Failed to parse command line options");
    }

    std::cout << "Config file: " << configFilePath << std::endl;
    std::cout << "Model root directory :" << modelsRootDir.c_str() << std::endl;
    std::cout << "Backend shared Library directory :" << backendSharedLibraryDir.c_str() << std::endl;

    s_configFilePath = configFilePath;
    s_modelRootDir = modelsRootDir;
    s_backendSharedLibraryDir = backendSharedLibraryDir;

    return session.run();
}

// Function to create the configuration file from CONFIG_FILE_PATH
LlmConfig SetupTestConfig()
{
    /* Ensure the config file and model root directories
    * have been provided. */
    REQUIRE(!s_configFilePath.empty());
    REQUIRE(!s_modelRootDir.empty());

    std::ifstream configFile(s_configFilePath);
    std::stringstream buffer;
    buffer << configFile.rdbuf();  // Read file into stringstream
    std::string jsonContent = buffer.str();

    LlmConfig configTest{jsonContent};
    std::string modelPath = s_modelRootDir + "/" + configTest.GetConfigString("llmModelName");
    configTest.SetConfigString("llmModelName", modelPath);

    // llama.cpp multimodal only
    if (!configTest.GetConfigString("projModelName").empty()) {
        std::string projModelPath = s_modelRootDir + "/" + configTest.GetConfigString("projModelName");
        configTest.SetConfigString("projModelName", projModelPath);
    }
    return configTest;
}

/**
 * Simple test to ensure we pick up the correct LLMImpl based on the modalities in the config
 */
TEST_CASE("LLM Factory test") {
    LlmConfig configTest = SetupTestConfig();
    LLM llm{};
    llm.LlmInit(configTest);
    std::vector<std::string> modalities = llm.SupportedInputModalities();
    if(configTest.GetConfigBool("isVision")) {
        CHECK(modalities.size() == 2);
    } else {
        CHECK(modalities.size() == 1);
    }

    llm.FreeLlm();
}

/**
 * Simple query->response test
 */
TEST_CASE("Test Llm-Wrapper class")
{
    LlmConfig configTest = SetupTestConfig();
    LLM llm{};
    std::stringstream stopWordsStream;
    std::list<std::string> stopWords;
    std::string question         = "What is the capital of France?" ;

    int circuitBreaker = 0;
    
    // Multimodal tests only
    if (configTest.GetConfigBool("isVision"))
    {
         // Validate the vision path can describe objects in images.
        SECTION("Describe Image")
        {
            llm.LlmInit(configTest, s_backendSharedLibraryDir);
            struct Case {
                const char* file;
                std::vector<std::string> expects;
            };

            const std::array<Case, 3> cases{{
                {"cat.bmp",   {"cat"}},
                {"tiger.bmp", {"tiger"}},
                {"dog.bmp",   {"dog", "puppy"}},
            }};

            bool isFirstMessage = true;
            for (const auto& c : cases) {
                std::string prompt = "Can you describe this image briefly?";
                LlmChat::Payload payload{prompt, std::string{TEST_RESOURCE_DIR} + "/" + c.file, isFirstMessage};
                std::string response;
                llm.Encode(payload);

                while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (LLM::endToken == s)
                        break;
                    response += s;

                    if (circuitBreaker++ > maxTokenRetrievalAttempts) {
                        FAIL("Token retrieval attempts exceed threshold, terminating test run (1)");
                    }
                }

                bool match = false;
                for (const auto& e : c.expects) {
                    if (response.find(e) != std::string::npos) {
                        match = true;
                        break;
                    }
                }
                CHECK(match);
                isFirstMessage = false;
            }

            llm.FreeLlm();
        }


        // Validate multi-turn context handling for a follow-up question after an image turn.
        SECTION("Follow Up Question")
        {
            llm.LlmInit(configTest, s_backendSharedLibraryDir);
            std::string prompt = "What type of dress can you see in this image?";

            LlmChat::Payload payload{prompt, std::string{TEST_RESOURCE_DIR} + "/" + "kimono.bmp", true};
            llm.Encode(payload);
            std::string response1;
            while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (LLM::endToken == s)
                        break;
                    response1 += s;

                    if (circuitBreaker++ > maxTokenRetrievalAttempts) {
                        FAIL("Token retrieval attempts exceed threshold, terminating test run (2)");
                    }
                }
            CHECK(response1.find("kimono") != std::string::npos);

            payload.textPrompt = "Which country does that dress belong to?";
            payload.isFirstMessage = false;
            payload.imagePath = "";
            std::string response2;

            llm.Encode(payload);
            while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (LLM::endToken == s)
                        break;
                    response2 += s;

                    if (circuitBreaker++ > maxTokenRetrievalAttempts) {
                        FAIL("Token retrieval attempts exceed threshold, terminating test run (3)");
                    }
                }
            CHECK(response2.find("Japan") != std::string::npos);
            llm.FreeLlm();
        }

        // Validate reset context.
        SECTION("Reset Context")
        {
            llm.LlmInit(configTest, s_backendSharedLibraryDir);
            std::string prompt =  "Can you describe this image?";

            LlmChat::Payload payload{prompt, std::string{TEST_RESOURCE_DIR} + "/" + "tiger.bmp", true};
            llm.Encode(payload);

            std::string response;
            while (llm.GetChatProgress() < 100) {
                    std::string s = llm.NextToken();
                    if (LLM::endToken == s)
                        break;
                    response += s;

                    if (circuitBreaker++ > maxTokenRetrievalAttempts) {
                        FAIL("Token retrieval attempts exceed threshold, terminating test run (4)");
                    }
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
                    if (LLM::endToken == s)
                        break;
                    response2 += s;

                    if (circuitBreaker++ > maxTokenRetrievalAttempts) {
                        FAIL("Token retrieval attempts exceed threshold, terminating test run (5)");
                    }
                }
            CHECK(response2.find("tiger") == std::string::npos);
            llm.FreeLlm();
        }
    }

    // Simple query
    SECTION("Simple Query Response")
    {
        std::string response;
        llm.LlmInit(configTest, s_backendSharedLibraryDir);
        LlmChat::Payload payload{question, "", true};
        llm.Encode(payload);
        while (llm.GetChatProgress() < 100) {
            std::string s = llm.NextToken();
            if (LLM::endToken == s)
                break;
            response += s;

            if (circuitBreaker++ > maxTokenRetrievalAttempts) {
                FAIL("Token retrieval attempts exceed threshold, terminating test run (6)");
            }
        }
        CHECK(response.find("Paris") != std::string::npos);
        llm.FreeLlm();
    }

    /**
     * Test Load Empty Model returns nullptr
     */
    SECTION("Test Load Empty Model")
    {
        std::string emptyString;
        configTest.SetConfigString("llmModelName", emptyString);
        REQUIRE_THROWS(llm.LlmInit(configTest, s_backendSharedLibraryDir));
        llm.FreeLlm();
    }

    llm.FreeLlm();
}
