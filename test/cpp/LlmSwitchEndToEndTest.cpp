//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"
#include "Logger.hpp"
#include "catch2/catch_test_macros.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

extern std::string s_configFilePath;
extern std::string s_modelRootDir;
extern std::string s_backendSharedLibraryDir;

static std::string DecodeTokensSimple(LLM &llm, int maxTokens)
{
    std::string output;
    output.reserve(256);

    for (int i = 0; i < maxTokens; ++i) {
        std::string token = llm.NextToken();
        if (token.empty()) {
            break;
        }
        output += token;
    }
    return output;
}

static LlmConfig LoadConfigFromFilePath(const std::string& path)
{
    std::filesystem::path configPath(path);
    if (configPath.is_relative()) {
        std::filesystem::path base(MODEL_CONFIG_DIR);
        if (configPath.string().rfind("config_files/model_configuration_files/", 0) == 0) {
            configPath = configPath.lexically_relative("config_files/model_configuration_files");
        }
        std::filesystem::path candidate = base / configPath;
        std::ifstream candidateStream(candidate);
        if (candidateStream.good()) {
            configPath = candidate;
        }
    }

    std::ifstream configFile(configPath);
    REQUIRE(configFile.good());

    std::stringstream buffer;
    buffer << configFile.rdbuf();
    LlmConfig config{buffer.str()};

    std::string modelPath =
        s_modelRootDir + "/" + config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
    config.SetConfigString(LlmConfig::ConfigParam::LlmModelName, modelPath);

    if (!config.GetConfigString(LlmConfig::ConfigParam::ProjModelName).empty()) {
        std::string projModelPath =
            s_modelRootDir + "/" + config.GetConfigString(LlmConfig::ConfigParam::ProjModelName);
        config.SetConfigString(LlmConfig::ConfigParam::ProjModelName, projModelPath);
    }

    return config;
}

static bool FileExists(const std::string& path)
{
    std::ifstream in(path);
    return in.good();
}

static bool IsNonEmptyFile(const std::filesystem::path& path)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return false;
    }
    const auto size = std::filesystem::file_size(path, ec);
    return !ec && size > 0;
}

static bool HasMnnArtifacts(const std::string& modelPath)
{
    std::error_code ec;
    const std::filesystem::path dir(modelPath);
    if (!std::filesystem::is_directory(dir, ec)) {
        return false;
    }

    const auto llmFile = dir / "llm.mnn";
    const auto weightFile = dir / "llm.mnn.weight";
    const auto embeddingsFile = dir / "embeddings_bf16.bin";
    const auto tokenizerFile = dir / "tokenizer.txt";

    return IsNonEmptyFile(llmFile) &&
           IsNonEmptyFile(weightFile) &&
           IsNonEmptyFile(embeddingsFile) &&
           IsNonEmptyFile(tokenizerFile);
}

TEST_CASE("LLM Switch: End-to-end text then switch backend")
{
    REQUIRE(!s_configFilePath.empty());
    REQUIRE(!s_modelRootDir.empty());

    const std::string question = "What is the capital of France?";

    LlmConfig firstConfig = LoadConfigFromFilePath(s_configFilePath);
    LLM llm{};

    llm.LlmInit(firstConfig, s_backendSharedLibraryDir);
    LOG_WARN("Initial backend active: %s", llm.GetFrameworkType().c_str());

    LlmChat::Payload payload{question, "", true};
    llm.Encode(payload);

    std::string response = DecodeTokensSimple(llm, 8);
    CHECK(response.find("Paris") != std::string::npos);

    const std::string initialBackend = llm.GetFrameworkType();

    std::vector<std::pair<std::string, std::string>> candidates;
#if defined(LLM_BACKEND_ONNXRUNTIME_GENAI)
    candidates.emplace_back("config_files/model_configuration_files/onnxrtTextConfig-phi-4.json",
                            "onnxruntime-genai");
#endif
#if defined(LLM_BACKEND_MNN)
    candidates.emplace_back("config_files/model_configuration_files/mnnTextConfig-llama-3.2-1B.json",
                            "mnn");
#endif
#if defined(LLM_BACKEND_MEDIAPIPE)
    candidates.emplace_back("config_files/model_configuration_files/mediapipeTextConfig-gemma-2B.json",
                            "mediapipe");
#endif
#if defined(LLM_BACKEND_EXECUTORCH)
    candidates.emplace_back("config_files/model_configuration_files/executorchTextConfig-llama-3.2-1B.json",
                            "executorch");
#endif
#if defined(LLM_BACKEND_LLAMA_CPP)
    candidates.emplace_back("config_files/model_configuration_files/llamaTextConfig-phi-2.json",
                            "llama.cpp");
#endif

    bool switched = false;
    for (const auto& candidate : candidates) {
        if (candidate.second == initialBackend) {
            continue;
        }

        LlmConfig switchConfig = LoadConfigFromFilePath(candidate.first);
        switchConfig.SetConfigString(LlmConfig::ConfigParam::Framework, candidate.second);

        const std::string modelPath =
            switchConfig.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
        if (!FileExists(modelPath)) {
            LOG_WARN("Skipping backend switch to %s; model missing: %s",
                     candidate.second.c_str(), modelPath.c_str());
            continue;
        }
        if (candidate.second == "mnn" && !HasMnnArtifacts(modelPath)) {
            LOG_WARN("Skipping backend switch to %s; missing or empty MNN artifacts under: %s",
                     candidate.second.c_str(), modelPath.c_str());
            continue;
        }

        const std::string projPath =
            switchConfig.GetConfigString(LlmConfig::ConfigParam::ProjModelName);
        if (!projPath.empty() && !FileExists(projPath)) {
            LOG_WARN("Skipping backend switch to %s; projection model missing: %s",
                     candidate.second.c_str(), projPath.c_str());
            continue;
        }

        llm.FreeLlm();
        llm.LlmInit(switchConfig, s_backendSharedLibraryDir);
        LOG_WARN("Switched backend active: %s", llm.GetFrameworkType().c_str());

        LlmChat::Payload secondPayload{question, "", true};
        llm.Encode(secondPayload);

        std::string response2 = DecodeTokensSimple(llm, 8);
        CHECK(response2.find("Paris") != std::string::npos);
        CHECK(llm.GetFrameworkType() == candidate.second);
        switched = true;
    }

    if (!switched) {
        LOG_WARN("No alternate backend switch performed in end-to-end switch test");
    }

    llm.FreeLlm();
}
