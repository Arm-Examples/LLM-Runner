//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "BenchRunner.hpp"
#include "Logger.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

using nlohmann::json;

namespace {

std::string FormatModelSize(uintmax_t sizeBytes)
{
    constexpr double bytesPerGb = 1000.0 * 1000.0 * 1000.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2)
        << (static_cast<double>(sizeBytes) / bytesPerGb) << " GB";
    return oss.str();
}

}  // namespace

BenchRunner::BenchRunner(IBenchAdapter& bench, const BenchRunConfig& config)
    : m_bench(bench)
    , m_config(config)
{}

int BenchRunner::Run(BenchReport& report)
{
    if (m_config.measuredIterations <= 0) {
        LOG_ERROR("Measured iterations must be positive");
        return 1;
    }

    report = BenchReport{};
    report.config = m_config;
    report.modelSizeBytes = m_bench.GetModelSizeBytes();

    report.results.reserve(static_cast<size_t>(m_config.measuredIterations));
    if (m_config.warmupIterations > 0) {
        LOG_INF("Running %d warmup iteration(s) (results ignored)...", m_config.warmupIterations);
    }
    const int totalIterations = m_config.warmupIterations + m_config.measuredIterations;
    for (int iter = 0; iter < totalIterations; ++iter) {
        const bool isMeasured = (iter >= m_config.warmupIterations);
        const auto encode = m_bench.EncodeStep();
        BenchDecodeStepResult decode{};
        decode.decodeTimeSec = LlmBench::MeasureTimingSec(
            isMeasured ? "bench_runner.decode_total.measure" : "bench_runner.decode_total.warmup",
            [&]() {
                for (int tokenIdx = 0; tokenIdx < m_bench.GetOutputTokens(); ++tokenIdx) {
                    const auto step = m_bench.DecodeStep();
                    decode.tokensGenerated += step.tokensGenerated;
                    if (tokenIdx == 0) {
                        decode.firstTokenFromDecodeStartMs = step.firstTokenFromDecodeStartMs;
                    }
                }
            });
        m_bench.StopGeneration();
        const BenchIterationResult result = m_bench.BuildIterationResult(encode, decode);
        if (isMeasured) {
            report.results.push_back(result);
        }
        m_bench.FinishIteration();
    }

    report.summary = ComputeSummaryStats(report.results);
    return 0;
}

BenchSummaryStats BenchRunner::ComputeSummaryStats(const std::vector<BenchIterationResult>& results)
{
    BenchSummaryStats stats{};
    if (results.empty()) {
        return stats;
    }

    const double n = static_cast<double>(results.size());
    for (const auto& ir : results) {
        stats.mean.timeToFirstTokenMs += ir.timeToFirstTokenMs;
        stats.mean.totalTimeMs += ir.totalTimeMs;
        stats.mean.encodeTokensPerSec += ir.encodeTokensPerSec;
        stats.mean.decodeTokensPerSec += ir.decodeTokensPerSec;
    }
    stats.mean.timeToFirstTokenMs /= n;
    stats.mean.totalTimeMs /= n;
    stats.mean.encodeTokensPerSec /= n;
    stats.mean.decodeTokensPerSec /= n;

    for (const auto& ir : results) {
        stats.stddev.timeToFirstTokenMs += std::pow(ir.timeToFirstTokenMs - stats.mean.timeToFirstTokenMs, 2.0);
        stats.stddev.totalTimeMs += std::pow(ir.totalTimeMs - stats.mean.totalTimeMs, 2.0);
        stats.stddev.encodeTokensPerSec += std::pow(ir.encodeTokensPerSec - stats.mean.encodeTokensPerSec, 2.0);
        stats.stddev.decodeTokensPerSec += std::pow(ir.decodeTokensPerSec - stats.mean.decodeTokensPerSec, 2.0);
    }
    stats.stddev.timeToFirstTokenMs = std::sqrt(stats.stddev.timeToFirstTokenMs / n);
    stats.stddev.totalTimeMs = std::sqrt(stats.stddev.totalTimeMs / n);
    stats.stddev.encodeTokensPerSec = std::sqrt(stats.stddev.encodeTokensPerSec / n);
    stats.stddev.decodeTokensPerSec = std::sqrt(stats.stddev.decodeTokensPerSec / n);
    return stats;
}

std::string BenchRunner::FormatText(const BenchReport& report,
                                    const std::string& modelPath,
                                    int contextSize,
                                    int numThreads,
                                    int numInputTokens,
                                    int numOutputTokens,
                                    const std::string& frameworkType)
{
    std::ostringstream oss;
    const std::string modelSize = FormatModelSize(report.modelSizeBytes);

    oss << "\n=== ARM LLM Benchmark ===\n\n";
    oss << "Parameters:\n";
    oss << "  model_path         : " << modelPath << "\n";
    oss << "  model_size         : " << modelSize << "\n";
    oss << "  num_input_tokens   : " << numInputTokens << "\n";
    oss << "  num_output_tokens  : " << numOutputTokens << "\n";
    oss << "  context_size       : " << contextSize << "\n";
    oss << "  num_threads        : " << numThreads << "\n";
    oss << "  num_iterations     : " << report.config.measuredIterations << "\n";
    oss << "  num_warmup         : " << report.config.warmupIterations << "\n\n";

    auto pad = [](const std::string& s, std::size_t width) {
        std::string out = s;
        if (s.find("±") != std::string::npos) {
            width += 1;
        }
        if (out.size() >= width) {
            return out.substr(0, width);
        }
        return out + std::string(width - out.size(), ' ');
    };

    auto formatPerf = [](double mean, double stddev, const char* unit) {
        std::ostringstream s;
        s << std::fixed;
        s << std::setw(9) << std::setprecision(3) << mean;
        s << " ± ";
        s << std::setw(6) << std::setprecision(3) << stddev;
        s << " (" << unit << ")";
        return s.str();
    };

    constexpr std::size_t COL_FW = 18;
    constexpr std::size_t COL_TH = 7;
    constexpr std::size_t COL_TEST = 6;
    constexpr std::size_t COL_PERF = 26;

    const auto& mean = report.summary.mean;
    const auto& stddev = report.summary.stddev;
    const std::string threadsStr = std::to_string(numThreads);

    oss << "\n======= Results =========\n\n";
    oss << "| " << pad("Framework", COL_FW)
        << " | " << pad("Threads", COL_TH)
        << " | " << pad("Test", COL_TEST)
        << " | " << pad("Performance", COL_PERF) << " |\n";
    oss << "| " << std::string(COL_FW, '-')
        << " | " << std::string(COL_TH, '-')
        << " | " << std::string(COL_TEST, '-')
        << " | " << std::string(COL_PERF, '-') << " |\n";
    oss << "| " << pad(frameworkType, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("pp" + std::to_string(numInputTokens), COL_TEST)
        << " | " << pad(formatPerf(mean.encodeTokensPerSec, stddev.encodeTokensPerSec, "t/s"), COL_PERF) << " |\n";
    oss << "| " << pad(frameworkType, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("tg" + std::to_string(numOutputTokens), COL_TEST)
        << " | " << pad(formatPerf(mean.decodeTokensPerSec, stddev.decodeTokensPerSec, "t/s"), COL_PERF) << " |\n";
    oss << "| " << pad(frameworkType, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("TTFT", COL_TEST)
        << " | " << pad(formatPerf(mean.timeToFirstTokenMs, stddev.timeToFirstTokenMs, "ms"), COL_PERF) << " |\n";
    oss << "| " << pad(frameworkType, COL_FW)
        << " | " << pad(threadsStr, COL_TH)
        << " | " << pad("Total", COL_TEST)
        << " | " << pad(formatPerf(mean.totalTimeMs, stddev.totalTimeMs, "ms"), COL_PERF) << " |\n";

    return oss.str();
}

std::string BenchRunner::FormatJson(const BenchReport& report,
                                    const std::string& modelPath,
                                    int contextSize,
                                    int numThreads,
                                    int numInputTokens,
                                    int numOutputTokens,
                                    const std::string& frameworkType)
{
    const auto round3 = [](double v) {
        return std::round(v * 1000.0) / 1000.0;
    };
    const std::string modelSize = FormatModelSize(report.modelSizeBytes);

    json out;
    out["parameters"] = {
        {"model_path", modelPath},
        {"model_size", modelSize},
        {"num_input_tokens", numInputTokens},
        {"num_output_tokens", numOutputTokens},
        {"context_size", contextSize},
        {"num_threads", numThreads},
        {"num_iterations", report.config.measuredIterations},
        {"num_warmup", report.config.warmupIterations},
    };
    out["framework"] = frameworkType;
    out["results"] = {
        {"mean", {
            {"encode_tokens_per_sec", round3(report.summary.mean.encodeTokensPerSec)},
            {"decode_tokens_per_sec", round3(report.summary.mean.decodeTokensPerSec)},
            {"ttft_ms", round3(report.summary.mean.timeToFirstTokenMs)},
            {"total_ms", round3(report.summary.mean.totalTimeMs)},
        }},
        {"stddev", {
            {"encode_tokens_per_sec", round3(report.summary.stddev.encodeTokensPerSec)},
            {"decode_tokens_per_sec", round3(report.summary.stddev.decodeTokensPerSec)},
            {"ttft_ms", round3(report.summary.stddev.timeToFirstTokenMs)},
            {"total_ms", round3(report.summary.stddev.totalTimeMs)},
        }},
    };

    out["iterations"] = json::array();
    for (const auto& ir : report.results) {
        out["iterations"].push_back({
            {"time_to_first_token_ms", round3(ir.timeToFirstTokenMs)},
            {"total_time_ms", round3(ir.totalTimeMs)},
            {"tokens_generated", ir.tokensGenerated},
            {"encode_time_sec", round3(ir.encodeTimeSec)},
            {"decode_time_sec", round3(ir.decodeTimeSec)},
            {"encode_tokens_per_sec", round3(ir.encodeTokensPerSec)},
            {"decode_tokens_per_sec", round3(ir.decodeTokensPerSec)},
        });
    }

    return out.dump();
}
