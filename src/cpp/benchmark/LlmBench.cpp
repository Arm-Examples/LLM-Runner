//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmBench.hpp"
#include "Logger.hpp"

#include <chrono>
#include <filesystem>

using namespace std::chrono;

namespace {

uintmax_t ValidateAndComputeModelSizeBytes(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) {
        THROW_ERROR("Configured model path does not exist: %s", path.string().c_str());
    }

    if (std::filesystem::is_regular_file(path)) {
        const uintmax_t sizeBytes = std::filesystem::file_size(path);
        if (sizeBytes == 0) {
            THROW_ERROR("Configured model file is empty: %s. This may indicate an incomplete download.", path.string().c_str());
        }
        return sizeBytes;
    }

    if (std::filesystem::is_directory(path)) {
        uintmax_t totalSizeBytes = 0;
        bool hasRegularFiles = false;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            hasRegularFiles = true;
            totalSizeBytes += entry.file_size();
        }
        if (!hasRegularFiles || totalSizeBytes == 0) {
            THROW_ERROR("Configured model directory is empty: %s. This may indicate an incomplete download.", path.string().c_str());
        }
        return totalSizeBytes;
    }

    THROW_ERROR("Configured model path is neither a regular file nor a directory: %s", path.string().c_str());
}

}  // namespace

LlmBench::LlmBench(LLM& llm, int numInputTokens, int numOutputTokens)
    : m_llm(llm)
    , m_numInputTokens(numInputTokens)
    , m_numOutputTokens(numOutputTokens)
    , m_frameworkType(LLM::GetFrameworkType())
{}

double LlmBench::MeasureTimingSec(const std::string& tag, const std::function<void()>& operation)
{
    const auto tStart = steady_clock::now();
    operation();
    const auto tEnd = steady_clock::now();
    const double elapsedSec = duration<double>(tEnd - tStart).count();
    LOG_DEBUG("[%s] elapsed=%.6f sec", tag.c_str(), elapsedSec);
    return elapsedSec;
}

int LlmBench::Initialize(const std::string& modelPath,
                         int numThreads,
                         int contextSize,
                         const std::string& sharedLibraryPath)
{
    if (m_numInputTokens <= 0 || m_numOutputTokens <= 0 || numThreads <= 0 || contextSize <= 0 || modelPath.empty()) {
        LOG_ERROR("Invalid benchmark settings: model='%s' input=%d output=%d threads=%d context=%d",
                  modelPath.c_str(),
                  m_numInputTokens,
                  m_numOutputTokens,
                  numThreads,
                  contextSize);
        return 1;
    }

    const int requiredTokens = m_numInputTokens + m_numOutputTokens;
    if (contextSize <= requiredTokens) {
        LOG_ERROR("context_size (%d) must be greater than num_input_tokens + num_output_tokens (%d + %d = %d).",
                  contextSize, m_numInputTokens, m_numOutputTokens, requiredTokens);
        return 1;
    }

    try {
        m_modelSizeBytes = ValidateAndComputeModelSizeBytes(modelPath);

        LlmConfig config(R"JSON(
            {
                "chat" : {
                    "systemPrompt": "",
                    "applyDefaultChatTemplate": true,
                    "systemTemplate" : "%s",
                    "userTemplate"   : "%s"
                },
                "model" : {
                    "llmModelName" : "",
                    "isVision" : false
                },
                "runtime" : {
                    "batchSize" : 256,
                    "numThreads" : 1,
                    "contextSize" : 2048
                },
                "stopWords": ["endoftext"]
            }
        )JSON");
        config.SetConfigString(LlmConfig::ConfigParam::LlmModelName, modelPath);
        config.SetConfigInt(LlmConfig::ConfigParam::NumThreads, numThreads);
        config.SetConfigInt(LlmConfig::ConfigParam::ContextSize, contextSize);

        m_llm.LlmInit(config, sharedLibraryPath);
        m_frameworkType = LLM::GetFrameworkType();
        PreparePayload();
    } catch (const std::exception& ex) {
        LOG_ERROR("LlmBench initialization failed: %s", ex.what());
        return 1;
    } catch (...) {
        LOG_ERROR("LlmBench initialization failed: unknown error");
        return 1;
    }

    return 0;
}

void LlmBench::PreparePayload()
{
    m_payload.textPrompt = m_llm.GeneratePromptWithNumTokens(static_cast<size_t>(m_numInputTokens));
    m_llm.ResetContext();
}

BenchEncodeStepResult LlmBench::EncodeStep()
{
    BenchEncodeStepResult result{};
    result.encodeTimeSec = MeasureTimingSec("bench_adapter.encode", [&]() {
        m_llm.Encode(m_payload);
    });
    return result;
}

BenchDecodeStepResult LlmBench::DecodeStep()
{
    BenchDecodeStepResult result{};
    result.tokensGenerated = 1;
    result.decodeTimeSec = MeasureTimingSec("bench_adapter.decode_step", [&]() {
        const std::string token = m_llm.NextToken();
        (void)token;
    });
    result.firstTokenFromDecodeStartMs = result.decodeTimeSec * 1000.0;
    return result;
}

BenchIterationResult LlmBench::BuildIterationResult(const BenchEncodeStepResult& encodeResult,
                                                    const BenchDecodeStepResult& decodeResult,
                                                    int numInputTokens,
                                                    const std::string& frameworkType)
{
    BenchIterationResult out{};
    out.encodeTimeSec = encodeResult.encodeTimeSec;
    out.decodeTimeSec = decodeResult.decodeTimeSec;
    out.tokensGenerated = decodeResult.tokensGenerated;
    out.timeToFirstTokenMs = (encodeResult.encodeTimeSec * 1000.0) + decodeResult.firstTokenFromDecodeStartMs;
    out.totalTimeMs = (encodeResult.encodeTimeSec + decodeResult.decodeTimeSec) * 1000.0;

    if (out.encodeTimeSec > 0.0) {
        if (frameworkType == "mediapipe") {
            out.encodeTokensPerSec = 1000.0 * (static_cast<double>(numInputTokens) / out.timeToFirstTokenMs);
        } else {
            out.encodeTokensPerSec = static_cast<double>(numInputTokens) / out.encodeTimeSec;
        }
    }

    if (out.decodeTimeSec > 0.0 && out.tokensGenerated > 0) {
        out.decodeTokensPerSec = static_cast<double>(out.tokensGenerated) / out.decodeTimeSec;
    }

    return out;
}

BenchIterationResult LlmBench::BuildIterationResult(const BenchEncodeStepResult& encodeResult,
                                                    const BenchDecodeStepResult& decodeResult) const
{
    return BuildIterationResult(encodeResult, decodeResult, m_numInputTokens, m_frameworkType);
}

void LlmBench::StopGeneration()
{
    m_llm.StopGeneration();
}

void LlmBench::FinishIteration()
{
    m_llm.ResetContext();
}
