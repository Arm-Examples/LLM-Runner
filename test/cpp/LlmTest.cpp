//
// SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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
#include <cctype>

#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_session.hpp"

#if defined(DEPRECATED)
#undef DEPRECATED
#endif /* defined(DEPRECATED) */


std::string s_configFilePath;
std::string s_modelRootDir;
std::string s_resourceRootDir;
std::string s_backendSharedLibraryDir;
bool s_debugResponses{false};
std::string s_transcriptPath;

static int maxTokenRetrievalAttempts = 10000;
static int testCtxLength = 80;       // Arbitrary truncated value to emulate faster end  of context.
static int testBatchLength = 64;     // The test batch for text should be fixed to avoid errors,
                                     // when truncating Context length to low values
static int testImgBatchLength = 256; // The mtmd requires text modality batch-sized to be fixed.

using namespace Catch::Clara;

int main(int argc, char* argv[])
{
    Catch::Session session;

    if (const char* env = std::getenv("LLM_TEST_DEBUG_RESPONSES")) {
        std::string v(env);
        for (auto& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (v == "1" || v == "true" || v == "yes" || v == "on") {
            s_debugResponses = true;
        }
    }
    if (const char* env = std::getenv("LLM_TEST_TRANSCRIPT_PATH")) {
        s_transcriptPath = env;
    }

    auto cli = session.cli() |
        Opt(s_configFilePath, "configFile")["--config"]
            ("Path to LLM runtime configuration JSON file") |
        Opt(s_modelRootDir, "modelRootDir")["--model-root"]
            ("Directory containing LLM model files") |
        Opt(s_resourceRootDir, "resourceRootDir")["--resource-root"]
            ("Directory containing LLM test resource files") |
        Opt(s_backendSharedLibraryDir, "sharedLibraryDir")["--backend-shared-lib-dir"]
            ("Directory containing backend shared libraries") |
        Opt(s_debugResponses)["--debug-responses"]
            ("Print prompts/responses and config summary to stdout (also supports env: LLM_TEST_DEBUG_RESPONSES=1)") |
        Opt(s_transcriptPath, "path")["--transcript"]
            ("Append prompts/responses to a transcript file (also supports env: LLM_TEST_TRANSCRIPT_PATH)");

    session.cli(cli);

    if (0 != session.applyCommandLine(argc, argv)) {
        LOG_ERROR("Failed to parse command-line options");
    }

    std::cout << "Config file: " << s_configFilePath << std::endl;
    std::cout << "Model root directory: " << s_modelRootDir << std::endl;
    std::cout << "Test resources directory: " << s_resourceRootDir << std::endl;
    std::cout << "Backend shared library directory: " << s_backendSharedLibraryDir << std::endl;
    std::cout << "Debug responses: " << s_debugResponses << std::endl;

    return session.run();
}

static void DebugPrint(const std::string& label, const std::string& value)
{

    if (s_debugResponses) {
        std::cout << label << value << std::endl;
    }
    if (s_transcriptPath.empty()) {
        return;
    }
    std::ofstream out(s_transcriptPath, std::ios::app);
    if (!out) {
        return;
    }
    out << label << value << "\n";
}

static void TranscriptSeparator()
{
    if (s_transcriptPath.empty()) {
        return;
    }
    std::ofstream out(s_transcriptPath, std::ios::app);
    if (!out) {
        return;
    }
    out << "----\n";
}

/**
 * Helper: Decode tokens from the LLM until chat progress reaches 100%
 * or the EOS token is emitted.
 *
 * A circuit breaker prevents infinite loops in cases where the LLM
 * stops producing tokens but does not signal EOS.
 *
 * @param llm  LLM instance
 * @param testId Identifier for reporting which test failed
 * @param maxTokens Maximum number of tokens to decode
 * @return Combined decoded output string
 */
static std::string DecodeTokens(LLM &llm, int testId, int maxTokens = maxTokenRetrievalAttempts)
{
    std::string output;
    int circuitBreaker = 0; // Reset for each decode operation

    while (llm.GetChatProgress() < 100) {
        if (circuitBreaker >= maxTokenRetrievalAttempts) {
            FAIL("Token retrieval attempts exceeded safety threshold in DecodeTokens() [Test "
                 + std::to_string(testId) + "]");
        }

        if (circuitBreaker >= maxTokens) {
            LOG_INF("DecodeTokens stopped after reaching the configured limit of %d tokens for test %d",
                    maxTokens,
                    testId);
            break;
        }

        const auto tokenId = llm.NextTokenId();
        if (!tokenId.has_value()) {
            break;
        }

        const std::string tok = llm.DetokenizeTextToken(*tokenId);
        if (llm.IsStopTextPiece(tok)) {
            break;
        }

        output += tok;

        ++circuitBreaker;
    }
    return output;
}

/**
 * Load the test configuration JSON and expand model paths relative to model-root directory.
 */
LlmConfig SetupTestConfig()
{
    REQUIRE(!s_configFilePath.empty());
    REQUIRE(!s_modelRootDir.empty());

    std::ifstream configFile(s_configFilePath);
    std::stringstream buffer;
    buffer << configFile.rdbuf();
    std::string jsonContent = buffer.str();

    LlmConfig configTest{jsonContent};
    std::string modelPath =
        s_modelRootDir + "/" + configTest.GetConfigString(LlmConfig::ConfigParam::LlmModelName);

    configTest.SetConfigString(LlmConfig::ConfigParam::LlmModelName, modelPath);

    // Optional projection model (used for multimodal llama.cpp builds)
    if (!configTest.GetConfigString(LlmConfig::ConfigParam::ProjModelName).empty()) {
        std::string projModelPath =
            s_modelRootDir + "/" + configTest.GetConfigString(LlmConfig::ConfigParam::ProjModelName);
        configTest.SetConfigString(LlmConfig::ConfigParam::ProjModelName, projModelPath);
    }

    return configTest;
}

static std::string Truncate(std::string s, size_t maxLen)
{
    if (s.size() <= maxLen) {
        return s;
    }
    s.resize(maxLen);
    s += "...";
    return s;
}

static std::string ConfigSummary(const LlmConfig& config)
{
    std::ostringstream oss;
    oss << "framework=" << LLM::GetFrameworkType();
    oss << ", model=" << config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);

    const std::string proj = config.GetConfigString(LlmConfig::ConfigParam::ProjModelName);
    if (!proj.empty()) {
        oss << ", projModel=" << proj;
    }

    oss << ", isVision=" << (config.GetConfigBool(LlmConfig::ConfigParam::IsVision) ? "true" : "false");
    oss << ", contextSize=" << config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);
    oss << ", batchSize=" << config.GetConfigInt(LlmConfig::ConfigParam::BatchSize);
    oss << ", numThreads=" << config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    oss << ", applyDefaultChatTemplate="
        << (config.GetConfigBool(LlmConfig::ConfigParam::ApplyDefaultChatTemplate) ? "true" : "false");

    const std::string systemPrompt = config.GetConfigString(LlmConfig::ConfigParam::SystemPrompt);
    if (!systemPrompt.empty()) {
        oss << ", systemPrompt=" << Truncate(systemPrompt, 80);
    }

    oss << ", stopWords=" << config.GetStopWords().size();
    return oss.str();
}

/**
 * Ensure correct LLM implementation is selected based on supported modalities.
 */
TEST_CASE("LLM Factory: Validate supported input modalities")
{
    LlmConfig configTest = SetupTestConfig();
    LLM llm{};
    llm.LlmInit(configTest);

    auto modalities = llm.SupportedInputModalities();

    if (configTest.GetConfigBool(LlmConfig::ConfigParam::IsVision)) {
        CHECK(modalities.size() == 2);
    } else {
        CHECK(modalities.size() == 1);
    }

    llm.FreeLlm();
}


/**
 * Query/response and multimodal execution tests.
 */
TEST_CASE("LLM Wrapper: End-to-end text and vision tests")
{
    LlmConfig configTest = SetupTestConfig();

    LLM llm{};
    std::string question = "What is the capital of France?" ;

    INFO("Config file: " << s_configFilePath);
    INFO("Model root directory: " << s_modelRootDir);
    INFO("Test resources root directory: " << s_resourceRootDir);
    INFO("Backend shared library directory: " << s_backendSharedLibraryDir);
    INFO("Config summary: " << ConfigSummary(configTest));

    TranscriptSeparator();
    DebugPrint("Config file: ", s_configFilePath);
    DebugPrint("Model root directory: ", s_modelRootDir);
    DebugPrint("Test resources root directory: ", s_resourceRootDir);
    DebugPrint("Backend shared library directory: ", s_backendSharedLibraryDir);
    DebugPrint("Config summary: ", ConfigSummary(configTest));

    auto checkContextFullError = [](const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("context is full") != std::string::npos);
    };

    //
    // Multimodal tests (vision enabled)
    //
    if (configTest.GetConfigBool(LlmConfig::ConfigParam::IsVision))
    {

        REQUIRE(!s_resourceRootDir.empty());

        SECTION("Vision: Describe objects in images")
        {
            llm.LlmInit(configTest, s_backendSharedLibraryDir);
            struct Case {
                const char* file;
                std::vector<std::string> expects;
            };

            const std::array<Case, 3> cases {{
                {"cat.bmp",   {"cat"}},
                {"tiger.bmp", {"tiger"}},
                {"dog.bmp",   {"dog", "puppy", "pug"}},
            }};

            bool isFirstMessage = true;

            for (const auto& c : cases) {
                std::string prompt = "Can you describe this image briefly?";
                LlmChat::Payload payload{prompt, std::string{s_resourceRootDir} + "/" + c.file, isFirstMessage};

                CAPTURE(prompt);
                llm.Encode(payload);
                CAPTURE(payload.imagePath);
                std::string response = DecodeTokens(llm, 1);
                CAPTURE(c.file);
                INFO("Response: " << response);
                DebugPrint("Prompt: ", prompt);
                DebugPrint("Image: ", payload.imagePath);
                DebugPrint("Response: ", response);

                bool match = false;
                for (const auto& expect : c.expects) {
                    if (response.find(expect) != std::string::npos) {
                        match = true;
                        break;
                    }
                }

                CHECK(match);
                isFirstMessage = false;
            }

            llm.FreeLlm();
        }

        SECTION("Vision: Multi-turn follow-up question after image input")
        {
            llm.LlmInit(configTest, s_backendSharedLibraryDir);

            std::string prompt = "What type of dress can you see in this image?";
            LlmChat::Payload payload{prompt, std::string{s_resourceRootDir} + "/kimono.bmp", true};

            CAPTURE(prompt);
            llm.Encode(payload);
            CAPTURE(payload.imagePath);
            std::string response1 = DecodeTokens(llm, 2);
            INFO("Response: " << response1);
            DebugPrint("Prompt: ", prompt);
            DebugPrint("Image: ", payload.imagePath);
            DebugPrint("Response: ", response1);
            CHECK(response1.find("kimono") != std::string::npos);

            payload.textPrompt = "Which country does that dress belong to?";
            payload.isFirstMessage = false;
            payload.imagePath = "";

            CAPTURE(payload.textPrompt);
            llm.Encode(payload);
            std::string response2 = DecodeTokens(llm, 3);
            INFO("Response: " << response2);
            DebugPrint("Prompt: ", payload.textPrompt);
            DebugPrint("Response: ", response2);
            CHECK(response2.find("Japan") != std::string::npos);

            llm.FreeLlm();
        }

        SECTION("Vision: Reset context clears prior conversational history")
        {
            llm.LlmInit(configTest, s_backendSharedLibraryDir);

            LlmChat::Payload payload{
                "Can you describe this image?",
                std::string{s_resourceRootDir} + "/tiger.bmp",
                true
            };

            CAPTURE(payload.textPrompt);
            llm.Encode(payload);
            CAPTURE(payload.imagePath);
            std::string response = DecodeTokens(llm, 4);
            INFO("Response: " << response);
            DebugPrint("Prompt: ", payload.textPrompt);
            DebugPrint("Image: ", payload.imagePath);
            DebugPrint("Response: ", response);
            CHECK(response.find("tiger") != std::string::npos);

            llm.ResetContext();

            payload.textPrompt = "Tell me more about this image?";
            payload.imagePath = "";
            payload.isFirstMessage = false;

            CAPTURE(payload.textPrompt);
            llm.Encode(payload);
            std::string response2 = DecodeTokens(llm, 5);

            INFO("Response: " << response2);
            DebugPrint("Prompt: ", payload.textPrompt);
            DebugPrint("Response: ", response2);
            CHECK(response2.find("tiger") == std::string::npos);
            llm.FreeLlm();
        }

        SECTION("Vision: Context overflow during Encode should throw an error")
        {
            configTest.SetConfigInt(LlmConfig::ContextSize, testCtxLength);
            configTest.SetConfigInt(LlmConfig::BatchSize, testImgBatchLength);

            llm.LlmInit(configTest, s_backendSharedLibraryDir);

            LlmChat::Payload payload{
                "What type of dress can you see in this image?",
                std::string{s_resourceRootDir} + "/kimono.bmp",
                true
            };

            try {
                while (llm.GetChatProgress() < 100) {
                    CAPTURE(payload.textPrompt);
                    llm.Encode(payload);
                    payload.textPrompt = "Tell me more about this image?";
                    payload.isFirstMessage = false;
                }
            } catch (const std::runtime_error& e) {
                INFO("Exception: " << e.what());
                DebugPrint("Exception: ", e.what());
                checkContextFullError(e);
            }

            llm.FreeLlm();
        }
    }

    //
    // Pure text tests
    //
    SECTION("Text: Simple query/response")
    {
        llm.LlmInit(configTest, s_backendSharedLibraryDir);

        LlmChat::Payload payload{question, "", true};
        CAPTURE(question);
        llm.Encode(payload);

        std::string response = DecodeTokens(llm, 6);
        INFO("Response: " << response);
        DebugPrint("Prompt: ", question);
        DebugPrint("Response: ", response);
        CHECK(response.find("Paris") != std::string::npos);

        llm.FreeLlm();
    }

    SECTION("Error Handling: Loading an empty model path should fail")
    {
        configTest.SetConfigString(LlmConfig::ConfigParam::LlmModelName, "");
        REQUIRE_THROWS(llm.LlmInit(configTest, s_backendSharedLibraryDir));
        llm.FreeLlm();
    }
    //
    SECTION("Context Overflow: Encode should throw when prompt exceeds available space")
    {
        configTest.SetConfigInt(LlmConfig::ContextSize, testCtxLength);
        configTest.SetConfigInt(LlmConfig::BatchSize, testBatchLength);
        llm.LlmInit(configTest, s_backendSharedLibraryDir);

        int count = 8;
        std::string longQuestion;
        longQuestion.reserve(question.size() * count);

        for (size_t i = 0; i < count; i++) {
            longQuestion.append(question);
        }

        LlmChat::Payload payload{longQuestion, "", true};

        try {
            llm.Encode(payload);
        } catch (const std::runtime_error& e) {
            INFO("Exception: " << e.what());
            DebugPrint("Exception: ", e.what());
            checkContextFullError(e);
        }
        llm.FreeLlm();
    }

    SECTION("Context Overflow: Decode path should also detect overflows")
    {
        configTest.SetConfigInt(LlmConfig::ContextSize, testCtxLength);
        llm.LlmInit(configTest, s_backendSharedLibraryDir);

        LlmChat::Payload payload{question, "", true};

        while (llm.GetChatProgress() < 100) {
            try {
                llm.Encode(payload);
                payload.isFirstMessage = false;
                DecodeTokens(llm, 7);
            } catch (const std::runtime_error& e) {
                INFO("Exception: " << e.what());
                DebugPrint("Exception: ", e.what());
                checkContextFullError(e);
                break;
            }
        }

        llm.ResetContext();
        payload.textPrompt = question;
        REQUIRE_NOTHROW(llm.Encode(payload));

        llm.FreeLlm();
    }

    llm.FreeLlm();
}

TEST_CASE("ONNX Runtime GenAI vision: text and sequential images") {
    LlmConfig configTest = SetupTestConfig();
    if (LLM::GetFrameworkType() != "onnxruntime-genai" ||
        !configTest.GetConfigBool(LlmConfig::ConfigParam::IsVision))
    {
        SKIP("This test requires the ONNX Runtime GenAI Vision configuration.");
    }

    constexpr int maxSmokeResponseTokens = 32;
    LLM llm{};
    llm.LlmInit(configTest, s_backendSharedLibraryDir);

    LlmChat::Payload textFirst{
        "Reply briefly so I know you are ready.",
        "",
        true
    };
    REQUIRE_NOTHROW(llm.Encode(textFirst));
    CHECK_FALSE(DecodeTokens(llm, 8, maxSmokeResponseTokens).empty());

    LlmChat::Payload firstImage{
        "Describe this image briefly.",
        std::string{TEST_RESOURCE_DIR} + "/tiger.bmp",
        false
    };
    REQUIRE_NOTHROW(llm.Encode(firstImage));
    CHECK_FALSE(DecodeTokens(llm, 9, maxSmokeResponseTokens).empty());

    LlmChat::Payload followUp{
        "What is the most notable object?",
        "",
        false
    };
    REQUIRE_NOTHROW(llm.Encode(followUp));
    CHECK_FALSE(DecodeTokens(llm, 10, maxSmokeResponseTokens).empty());

    LlmChat::Payload secondImage{
        "Describe this image too.",
        std::string{TEST_RESOURCE_DIR} + "/cat.bmp",
        false
    };
    REQUIRE_NOTHROW(llm.Encode(secondImage));
    CHECK_FALSE(DecodeTokens(llm, 11, maxSmokeResponseTokens).empty());

    LlmChat::Payload comparison{
        "Compare the two images briefly.",
        "",
        false
    };
    REQUIRE_NOTHROW(llm.Encode(comparison));
    CHECK_FALSE(DecodeTokens(llm, 12, maxSmokeResponseTokens).empty());

    llm.ResetContext();
    LlmChat::Payload imageAfterReset{
        "Describe this new image briefly.",
        std::string{TEST_RESOURCE_DIR} + "/dog.bmp",
        true
    };
    REQUIRE_NOTHROW(llm.Encode(imageAfterReset));
    CHECK_FALSE(DecodeTokens(llm, 13, maxSmokeResponseTokens).empty());

    llm.FreeLlm();
}
