//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmBench.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

using namespace std::chrono;

namespace
{

    double ComputeImageMegapixels(const std::uint64_t imagePixels)
    {
        return static_cast<double>(imagePixels) / 1000000.0;
    }

    BenchImageMetadata ToBenchImageMetadata(const LLM::InferenceStats &stats)
    {
        return {
            stats.originalImagePixels,
            stats.originalImageWidth,
            stats.originalImageHeight,
            stats.preparedImagePixels,
            stats.preparedImageWidth,
            stats.preparedImageHeight,
            stats.processedImagePixels,
            stats.processedImagePixelsSource,
        };
    }

    BenchTokenMetadata ToBenchTokenMetadata(const LLM::InferenceStats &stats)
    {
        return {
            stats.textPromptTokens,
            stats.visionTokens,
            stats.fusedPromptPositions,
        };
    }

    bool IsJsonConfigFile(const std::filesystem::path &path)
    {
        if (!std::filesystem::is_regular_file(path))
        {
            return false;
        }

        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });
        return extension == ".json";
    }

    std::string ReadFileToString(const std::filesystem::path &path)
    {
        std::ifstream in(path);
        if (!in)
        {
            THROW_ERROR("Failed to open model config JSON file: %s", path.string().c_str());
        }

        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

    std::filesystem::path ResolveModelRoot(const std::string &modelRootPath)
    {
        if (modelRootPath.empty())
        {
            return {};
        }

        const std::filesystem::path root = std::filesystem::path(modelRootPath).lexically_normal();
        if (!std::filesystem::is_directory(root))
        {
            THROW_INVALID_ARGUMENT("Model root directory does not exist: %s", root.string().c_str());
        }
        return root;
    }

    void ResolveRelativeConfigModelPath(LlmConfig &config,
                                        const LlmConfig::ConfigParam key,
                                        const std::filesystem::path &modelRootPath)
    {
        const std::string configuredPath = config.GetConfigString(key);
        if (configuredPath.empty())
        {
            return;
        }

        const std::filesystem::path modelPath(configuredPath);
        if (modelPath.is_absolute())
        {
            return;
        }

        if (modelRootPath.empty())
        {
            THROW_INVALID_ARGUMENT("Relative model path '%s' in config JSON requires --model-root", configuredPath.c_str());
        }

        config.SetConfigString(key, (modelRootPath / modelPath).lexically_normal().string());
    }

    void ResolveRelativeConfigModelPaths(LlmConfig &config, const std::string &modelRootPath)
    {
        const std::filesystem::path root = ResolveModelRoot(modelRootPath);
        ResolveRelativeConfigModelPath(config, LlmConfig::ConfigParam::LlmModelName, root);
        ResolveRelativeConfigModelPath(config, LlmConfig::ConfigParam::ProjModelName, root);
    }

    uintmax_t ComputeModelPathSizeBytes(const std::string &modelPath)
    {
        const std::filesystem::path path(modelPath);
        if (!std::filesystem::exists(path))
        {
            THROW_ERROR("Configured model path does not exist: %s", path.string().c_str());
        }

        if (std::filesystem::is_regular_file(path))
        {
            const uintmax_t sizeBytes = std::filesystem::file_size(path);
            if (sizeBytes == 0)
            {
                THROW_ERROR("Configured model file is empty: %s. This may indicate an incomplete download.", path.string().c_str());
            }
            return sizeBytes;
        }

        if (std::filesystem::is_directory(path))
        {
            uintmax_t totalSizeBytes = 0;
            bool hasRegularFiles = false;
            for (const auto &entry : std::filesystem::recursive_directory_iterator(path))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }
                hasRegularFiles = true;
                totalSizeBytes += entry.file_size();
            }
            if (!hasRegularFiles || totalSizeBytes == 0)
            {
                THROW_ERROR("Configured model directory is empty: %s. This may indicate an incomplete download.", path.string().c_str());
            }
            return totalSizeBytes;
        }

        THROW_ERROR("Configured model path is neither a regular file nor a directory: %s", path.string().c_str());
    }

    uintmax_t ComputeConfigModelSizeBytes(const LlmConfig &config)
    {
        const std::vector<std::string> configuredPaths{
            config.GetConfigString(LlmConfig::ConfigParam::LlmModelName),
            config.GetConfigString(LlmConfig::ConfigParam::ProjModelName),
        };

        uintmax_t totalSizeBytes = 0;
        std::set<std::string> uniquePaths;
        for (const auto &configuredPath : configuredPaths)
        {
            if (configuredPath.empty())
            {
                continue;
            }
            const auto normalizedPath = std::filesystem::path(configuredPath).lexically_normal().string();
            if (!uniquePaths.insert(normalizedPath).second)
            {
                continue;
            }
            totalSizeBytes += ComputeModelPathSizeBytes(normalizedPath);
        }

        if (totalSizeBytes == 0)
        {
            THROW_ERROR("Model config JSON did not resolve to any model artifacts");
        }
        return totalSizeBytes;
    }

    LlmConfig LoadVisionBenchConfig(const std::string &configPath,
                                    const std::string &modelRootPath,
                                    uintmax_t &modelSizeBytes)
    {
        if (!IsJsonConfigFile(configPath))
        {
            THROW_ERROR("Scenario image benchmarks require --model to be a JSON config file: %s", configPath.c_str());
        }

        LlmConfig config(ReadFileToString(configPath));
        ResolveRelativeConfigModelPaths(config, modelRootPath);
        if (!config.GetConfigBool(LlmConfig::ConfigParam::IsVision))
        {
            THROW_ERROR("Scenario image benchmarks require config.model.isVision=true: %s", configPath.c_str());
        }

        modelSizeBytes = ComputeConfigModelSizeBytes(config);
        return config;
    }

    LlmConfig BuildTextBenchConfig(const std::string &modelPath, uintmax_t &modelSizeBytes)
    {
        modelSizeBytes = ComputeModelPathSizeBytes(modelPath);
        LlmConfig textConfig(R"JSON(
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
        textConfig.SetConfigString(LlmConfig::ConfigParam::LlmModelName, modelPath);
        return textConfig;
    }

} // namespace

LlmBench::LlmBench(LLM &llm, const int numInputTokens, const int numOutputTokens)
    : m_llm(llm), m_numInputTokens(numInputTokens), m_numOutputTokens(numOutputTokens), m_frameworkType(LLM::GetFrameworkType())
{
    if (m_numInputTokens > 0)
    {
        m_workloadMetadata.inputTokens = m_numInputTokens;
    }
}

LlmBench::LlmBench(LLM &llm, const int numInputTokens, const int numOutputTokens, const BenchScenario &scenario)
    : LlmBench(llm, numInputTokens, numOutputTokens)
{
    m_scenario = scenario;
    m_payloadKind = ResolvePayloadKind(m_scenario);
    ConfigureScenarioWorkloadMetadata(scenario);
}

double LlmBench::MeasureTimingSec(const std::string &tag, const std::function<void()> &operation)
{
    const auto tStart = steady_clock::now();
    operation();
    const auto tEnd = steady_clock::now();
    const double elapsedSec = duration<double>(tEnd - tStart).count();
    LOG_DEBUG("[%s] elapsed=%.6f sec", tag.c_str(), elapsedSec);
    return elapsedSec;
}

LlmBench::PayloadKind LlmBench::ResolvePayloadKind(const std::optional<BenchScenario> &scenario)
{
    if (!scenario.has_value())
    {
        return PayloadKind::SyntheticText;
    }
    return scenario->HasImage() ? PayloadKind::ScenarioImage : PayloadKind::ScenarioText;
}

bool LlmBench::IsScenarioRun() const
{
    return !UsesSyntheticPrompt();
}

bool LlmBench::IsVisionRun() const
{
    return m_payloadKind == PayloadKind::ScenarioImage;
}

bool LlmBench::UsesSyntheticPrompt() const
{
    return m_payloadKind == PayloadKind::SyntheticText;
}

void LlmBench::ValidateInitializeArgs(const std::string &modelPath,
                                      const int numThreads,
                                      const int contextSize) const
{
    if (modelPath.empty() || m_numOutputTokens <= 0 || numThreads <= 0 || contextSize <= 0)
    {
        THROW_INVALID_ARGUMENT("Invalid benchmark settings: model='%s' input=%d output=%d threads=%d context=%d",
                               modelPath.c_str(),
                               m_numInputTokens,
                               m_numOutputTokens,
                               numThreads,
                               contextSize);
    }

    if (UsesSyntheticPrompt() && m_numInputTokens <= 0)
    {
        THROW_INVALID_ARGUMENT("Synthetic text benchmarks require num_input_tokens > 0");
    }

    const int benchmarkInputTokens = std::max(0, m_numInputTokens);
    const int requiredTokens = benchmarkInputTokens + m_numOutputTokens;
    if (contextSize <= requiredTokens)
    {
        THROW_INVALID_ARGUMENT("context_size (%d) must be greater than num_input_tokens + num_output_tokens (%d + %d = %d).",
                               contextSize,
                               m_numInputTokens,
                               m_numOutputTokens,
                               requiredTokens);
    }
}

LlmConfig LlmBench::BuildBenchmarkConfig(const std::string &modelPath, const std::string &modelRootPath)
{
    if (IsVisionRun())
    {
        return LoadVisionBenchConfig(modelPath, modelRootPath, m_modelSizeBytes);
    }
    return BuildTextBenchConfig(modelPath, m_modelSizeBytes);
}

void LlmBench::ApplyRuntimeConfig(LlmConfig &config, const int numThreads, const int contextSize)
{
    config.SetConfigInt(LlmConfig::ConfigParam::NumThreads, numThreads);
    config.SetConfigInt(LlmConfig::ConfigParam::ContextSize, contextSize);
}

void LlmBench::ConfigureScenarioWorkloadMetadata(const BenchScenario &scenario)
{
    // Scenario payload token count is not exposed by the backend-agnostic Encode() API.
    m_workloadMetadata.isScenario = true;
    m_workloadMetadata.workloadType = scenario.WorkloadType();
    m_workloadMetadata.imageCount = scenario.ImageCount();
    m_workloadMetadata.inputTokens.reset();
}

int LlmBench::Initialize(const std::string &modelPath,
                         const int numThreads,
                         const int contextSize,
                         const std::string &sharedLibraryPath,
                         const std::string &modelRootPath)
{
    try
    {
        ValidateInitializeArgs(modelPath, numThreads, contextSize);

        LlmConfig config = BuildBenchmarkConfig(modelPath, modelRootPath);
        ApplyRuntimeConfig(config, numThreads, contextSize);

        m_llm.LlmInit(config, sharedLibraryPath);
        m_frameworkType = LLM::GetFrameworkType();
        PreparePayload();
    }
    catch (const std::exception &ex)
    {
        LOG_ERROR("LlmBench initialization failed: %s", ex.what());
        return 1;
    }
    catch (...)
    {
        LOG_ERROR("LlmBench initialization failed: unknown error");
        return 1;
    }

    return 0;
}

void LlmBench::PreparePayload()
{
    m_payload = LlmChat::Payload{};
    m_workloadMetadata.imageMetadata = BenchImageMetadata{};
    m_workloadMetadata.tokenMetadata = BenchTokenMetadata{};
    m_hasObservedWorkloadMetadata = false;
    if (IsScenarioRun())
    {
        const BenchScenario &scenario = m_scenario.value();
        m_payload.textPrompt = scenario.prompt.value_or("");
        m_payload.imagePath = scenario.imagePath.value_or("");
        m_payload.isFirstMessage = true;
    }
    else
    {
        m_payload.textPrompt = m_llm.GeneratePromptWithNumTokens(static_cast<size_t>(m_numInputTokens));
    }
    m_llm.ResetContext();
}

void LlmBench::RecordEncodeMetadata(BenchEncodeStepResult &result)
{
    result.inferenceStats = m_llm.GetLastInferenceStats();
    if (m_hasObservedWorkloadMetadata)
    {
        return;
    }

    m_workloadMetadata.tokenMetadata = ToBenchTokenMetadata(result.inferenceStats);
    if (IsVisionRun())
    {
        m_workloadMetadata.imageMetadata = ToBenchImageMetadata(result.inferenceStats);
    }
    m_hasObservedWorkloadMetadata = true;
}

BenchEncodeStepResult LlmBench::EncodeStep()
{
    BenchEncodeStepResult result{};
    LlmChat::Payload payload = m_payload;
    result.encodeTimeSec = MeasureTimingSec("bench_adapter.encode", [&]
                                            {
        // QueryBuilder mutates payload text by adding chat/image markers.
        // Rebuild a fresh payload each iteration so multimodal markers do not accumulate.
        m_llm.Encode(payload, &result.inferenceStats); });
    RecordEncodeMetadata(result);
    return result;
}

BenchDecodeStepResult LlmBench::DecodeStep()
{
    BenchDecodeStepResult result{};
    result.decodeTimeSec = MeasureTimingSec("bench_adapter.decode_step", [&]() {
        const auto tokenId = m_llm.NextTokenId();
        if (!tokenId.has_value()) {
            result.tokensGenerated = 0;
            return;
        }

        // Benchmark one emitted output step end-to-end, including detokenization.
        (void)m_llm.DetokenizeTextToken(*tokenId);
        result.tokensGenerated = 1;
    });

    result.firstTokenFromDecodeStartMs = result.decodeTimeSec * 1000.0;
    return result;
}

BenchIterationResult LlmBench::BuildIterationResult(const BenchEncodeStepResult &encodeResult,
                                                    const BenchDecodeStepResult &decodeResult,
                                                    const int numInputTokens)
{
    BenchIterationResult out{};
    out.encodeTimeSec = encodeResult.encodeTimeSec;
    out.decodeTimeSec = decodeResult.decodeTimeSec;
    out.tokensGenerated = decodeResult.tokensGenerated;
    out.timeToFirstTokenMs = (encodeResult.encodeTimeSec * 1000.0) + decodeResult.firstTokenFromDecodeStartMs;
    out.totalTimeMs = (encodeResult.encodeTimeSec + decodeResult.decodeTimeSec) * 1000.0;

    if (numInputTokens > 0)
    {
        out.inputTokens = numInputTokens;
        if (out.encodeTimeSec > 0.0)
        {
            out.encodeTokensPerSec = static_cast<double>(numInputTokens) / out.encodeTimeSec;
        }
    }

    out.imageMetadata = ToBenchImageMetadata(encodeResult.inferenceStats);
    out.tokenMetadata = ToBenchTokenMetadata(encodeResult.inferenceStats);
    out.visionTimeMs = encodeResult.inferenceStats.visionTimeMs;
    out.prefillTimeMs = encodeResult.inferenceStats.prefillTimeMs;

    if (out.imageMetadata.preparedImagePixels.has_value() && out.encodeTimeSec > 0.0)
    {
        out.preparedImageMegapixelsPerEncodeSec =
            ComputeImageMegapixels(out.imageMetadata.preparedImagePixels.value()) / out.encodeTimeSec;
    }

    if (out.imageMetadata.processedImagePixels.has_value() &&
        out.visionTimeMs.has_value() &&
        out.visionTimeMs.value() > 0.0)
    {
        out.visionMegapixelsPerSec =
            ComputeImageMegapixels(out.imageMetadata.processedImagePixels.value()) /
            (out.visionTimeMs.value() / 1000.0);
    }

    if (out.decodeTimeSec > 0.0 && out.tokensGenerated > 0)
    {
        out.decodeTokensPerSec = static_cast<double>(out.tokensGenerated) / out.decodeTimeSec;
    }

    return out;
}

BenchIterationResult LlmBench::BuildIterationResult(const BenchEncodeStepResult &encodeResult,
                                                    const BenchDecodeStepResult &decodeResult) const
{
    BenchIterationResult result = BuildIterationResult(encodeResult, decodeResult, m_numInputTokens);
    if (!UsesSyntheticPrompt())
    {
        result.inputTokens.reset();
        result.encodeTokensPerSec.reset();
    }
    return result;
}

void LlmBench::StopGeneration()
{
    m_llm.StopGeneration();
}

void LlmBench::FinishIteration()
{
    m_llm.ResetContext();
}
