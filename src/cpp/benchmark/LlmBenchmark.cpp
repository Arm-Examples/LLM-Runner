//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmBenchmark.hpp"
#include "Logger.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>

using namespace std::chrono;

LlmBenchmark::~LlmBenchmark() {}

LlmBenchmark::LlmBenchmark(const std::string& modelPath,
                           int numInputTokens,
                           int numOutputTokens,
                           int numThreads,
                           int numIterations,
                           int numWarmupIterations,
                           const std::string& sharedLibraryPath)
    : m_modelPath(modelPath)
    , m_numInputTokens(numInputTokens)
    , m_numOutputTokens(numOutputTokens)
    , m_numThreads(numThreads)
    , m_numIterations(numIterations)
    , m_numWarmupIterations(numWarmupIterations)
    , m_sharedLibraryPath(sharedLibraryPath)
    , m_config(R"JSON(
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
    )JSON") {}

void LlmBenchmark::InitConfig()
{
    // Fill config path & threads
    m_config.SetConfigString(LlmConfig::ConfigParam::LlmModelName, m_modelPath);
    m_config.SetConfigInt(LlmConfig::ConfigParam::NumThreads, m_numThreads);
}

void LlmBenchmark::InitLlm()
{
    this->LlmInit(m_config, m_sharedLibraryPath);
    m_frameworkType = this->GetFrameworkType();
}

void LlmBenchmark::PreparePayload()
{
    LlmChat::Payload payload;
    payload.textPrompt = this->GeneratePromptWithNumTokens(m_numInputTokens);
    m_payload = payload;
    this->ResetContext();
}

LlmBenchmark::IterationResult LlmBenchmark::RunSingleIteration(int /*iterationIndex*/)
{
    IterationResult ir{};
    bool gotFirstToken = false;

    const auto tStart = steady_clock::now();
    auto tFirstToken  = tStart;

    // --- Encode phase ---
    const auto tEncodeStart = steady_clock::now();
    this->Encode(m_payload);
    const auto tEncodeEnd   = steady_clock::now();
    ir.encodeTimeSec = duration<double>(tEncodeEnd - tEncodeStart).count();

    // --- Decode phase ---
    const auto tDecodeStart = tEncodeEnd;
    int tokens = 0;
    while (tokens < m_numOutputTokens) {
        std::string token = this->NextToken();
        if (!gotFirstToken) {
            gotFirstToken = true;
            tFirstToken   = steady_clock::now();
        }
        ++tokens;
    }
    const auto tEnd = steady_clock::now();
    this->StopGeneration();

    ir.decodeTimeSec   = duration<double>(tEnd - tDecodeStart).count();
    ir.tokensGenerated = tokens;
    ir.timeToFirstTokenMs =
        gotFirstToken
            ? duration<double, std::milli>(tFirstToken - tStart).count()
            : duration<double, std::milli>(tEnd - tStart).count();
    ir.totalTimeMs =
        duration<double, std::milli>(tEnd - tStart).count();

    // tokens/sec (guard division)
    if (ir.encodeTimeSec > 0.0) {
        if(this->GetFrameworkType() == "mediapipe") {
            ir.encodeTokensPerSec = 1000 *(static_cast<double>(m_numInputTokens) / ir.timeToFirstTokenMs);
        } else {
            ir.encodeTokensPerSec = static_cast<double>(m_numInputTokens) / ir.encodeTimeSec;
        }
    }
    if (ir.decodeTimeSec > 0.0 && tokens > 0) {
        ir.decodeTokensPerSec = static_cast<double>(tokens) / ir.decodeTimeSec;
    }

    // Reset context for next iteration
    this->ResetContext();

    return ir;
}

std::string LlmBenchmark::GetResults() const
{
    if (m_results.empty()) {
        return "No benchmark results available. Run() has not been executed yet.\n";
    }

    const double n = static_cast<double>(m_results.size());

    // --- compute means ---
    double sumTTFT = 0.0, sumTotal = 0.0;
    double sumEncodeTPS = 0.0, sumDecodeTPS = 0.0;

    for (const auto& ir : m_results) {
        sumTTFT     += ir.timeToFirstTokenMs;
        sumTotal    += ir.totalTimeMs;
        sumEncodeTPS+= ir.encodeTokensPerSec;
        sumDecodeTPS+= ir.decodeTokensPerSec;
    }

    double meanTTFT      = sumTTFT / n;
    double meanTotal     = sumTotal / n;
    double meanEncodeTPS = sumEncodeTPS / n;
    double meanDecodeTPS = sumDecodeTPS / n;

    // --- compute stddev ---
    double varTTFT = 0.0, varTotal = 0.0;
    double varEncodeTPS = 0.0, varDecodeTPS = 0.0;

    for (const auto& ir : m_results) {
        varTTFT  += std::pow(ir.timeToFirstTokenMs - meanTTFT, 2);
        varTotal += std::pow(ir.totalTimeMs - meanTotal, 2);

        varEncodeTPS += std::pow(ir.encodeTokensPerSec - meanEncodeTPS, 2);
        varDecodeTPS += std::pow(ir.decodeTokensPerSec - meanDecodeTPS, 2);
    }

    double stdTTFT      = std::sqrt(varTTFT / n);
    double stdTotal     = std::sqrt(varTotal / n);
    double stdEncodeTPS = std::sqrt(varEncodeTPS / n);
    double stdDecodeTPS = std::sqrt(varDecodeTPS / n);

    // --- Format output ---
    std::ostringstream oss;
    oss << "\n=== ARM LLM Benchmark ===\n\n";
    oss << "Parameters:\n";
    oss << "  model_path         : " << m_modelPath << "\n";
    oss << "  num_input_tokens   : " << m_numInputTokens << "\n";
    oss << "  num_output_tokens  : " << m_numOutputTokens << "\n";
    oss << "  num_threads        : " << m_numThreads << "\n";
    oss << "  num_iterations     : " << m_numIterations << "\n";
    oss << "  num_warmup         : " << m_numWarmupIterations << "\n\n";

    // --- helper to pad/truncate strings ---
    auto pad = [](const std::string& s, std::size_t width) {
        std::string out = s;

        // UTF-8 "±" takes 2 bytes but prints as 1 char → adjust padding
        if (s.find("±") != std::string::npos) {
            width += 1;  // compensate for byte/char mismatch
        }

        if (out.size() >= width) {
            return out.substr(0, width);
        }
        return out + std::string(width - out.size(), ' ');
    };

    // fixed widths for each column
    constexpr std::size_t COL_FW   = 18;  // Framework
    constexpr std::size_t COL_TH   = 7;   // Threads
    constexpr std::size_t COL_TEST = 6;   // Test
    constexpr std::size_t COL_PERF = 26;  // Performance

    auto formatPerf = [](double mean, double stddev, const char* unit) {
        std::ostringstream s;

        // Assume up to 5 digits before decimal and 3 after: width ~ 9 is safe
        constexpr int MAIN_WIDTH = 9;   // e.g. "1939.910"
        constexpr int STD_WIDTH  = 6;   // e.g. "14.804"

        s << std::fixed;

        // Main value aligned
        s << std::setw(MAIN_WIDTH) << std::setprecision(3) << mean;
        s << " ± ";
        // Stddev aligned
        s << std::setw(STD_WIDTH) << std::setprecision(3) << stddev;
        s << " (" << unit << ")";

        return s.str();
    };


    oss << "\n======= Results =========\n\n";
    // Header row
    oss << "| " << pad("Framework", COL_FW)
        << " | " << pad("Threads", COL_TH)
        << " | " << pad("Test", COL_TEST)
        << " | " << pad("Performance", COL_PERF) << " |\n";

    // Separator row
    oss << "| " << std::string(COL_FW, '-')
        << " | " << std::string(COL_TH, '-')
        << " | " << std::string(COL_TEST, '-')
        << " | " << std::string(COL_PERF, '-') << " |\n";

    const std::string fw = m_frameworkType;
    const std::string threadsStr = std::to_string(m_numThreads);

    // ppN (encode)
    oss << "| " << pad(fw, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("pp" + std::to_string(m_numInputTokens), COL_TEST)
        << " | " << pad(formatPerf(meanEncodeTPS, stdEncodeTPS, "t/s"), COL_PERF)
        << " |\n";

    // tgN (decode)
    oss << "| " << pad(fw, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("tg" + std::to_string(m_numOutputTokens), COL_TEST)
        << " | " << pad(formatPerf(meanDecodeTPS, stdDecodeTPS, "t/s"), COL_PERF)
        << " |\n";

    // TTFT
    oss << "| " << pad(fw, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("TTFT", COL_TEST)
        << " | " << pad(formatPerf(meanTTFT, stdTTFT, "ms"), COL_PERF)
        << " |\n";

    // Total
    oss << "| " << pad(fw, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("Total", COL_TEST)
        << " | " << pad(formatPerf(meanTotal, stdTotal, "ms"), COL_PERF)
        << " |\n";

    return oss.str();
}

int LlmBenchmark::Run()
{
    try {
        InitConfig();
        InitLlm();
        PreparePayload();

        // Warmup phase (ignored in stats)
        if (m_numWarmupIterations > 0) {
            LOG_INF("Running %d warmup iteration(s) (results ignored)...", m_numWarmupIterations);
            for (int i = 0; i < m_numWarmupIterations; ++i) {
                // We intentionally ignore the result
                RunSingleIteration(-1);
            }
        }

        m_results.clear();
        m_results.reserve(m_numIterations);

        // Measured iterations
        for (int iter = 0; iter < m_numIterations; ++iter) {
            auto ir = RunSingleIteration(iter);
            m_results.push_back(ir);
        }
        return 0;
    } catch (const std::exception& ex) {
        LOG_ERROR("Benchmark failed: %s", ex.what());
        return 1;
    } catch (...) {
        LOG_ERROR("Benchmark failed: unknown error");
        return 1;
    }
}
