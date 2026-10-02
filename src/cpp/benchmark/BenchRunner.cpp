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
#include <optional>
#include <sstream>
#include <tuple>
#include <utility>

using nlohmann::json;

namespace
{

    constexpr std::size_t frameworkColumnWidth = 18;
    constexpr std::size_t threadsColumnWidth = 7;
    constexpr std::size_t testColumnWidth = 7;
    constexpr std::size_t performanceColumnWidth = 31;

    std::string FormatModelSize(const uintmax_t sizeBytes)
    {
        constexpr double bytesPerGb = 1000.0 * 1000.0 * 1000.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << (static_cast<double>(sizeBytes) / bytesPerGb) << " GB";
        return oss.str();
    }

    template <typename Accessor>
    std::pair<std::optional<double>, std::optional<double>> calculateOptionalStats(
        const std::vector<BenchIterationResult> &results,
        Accessor accessor)
    {
        double sum = 0.0;
        std::size_t count = 0;
        for (const auto &result : results)
        {
            const auto value = accessor(result);
            if (value.has_value())
            {
                sum += *value;
                ++count;
            }
        }
        if (count == 0)
        {
            return {std::nullopt, std::nullopt};
        }

        const double mean = sum / static_cast<double>(count);
        double squaredDifferenceSum = 0.0;
        for (const auto &result : results)
        {
            const auto value = accessor(result);
            if (value.has_value())
            {
                squaredDifferenceSum += std::pow(*value - mean, 2.0);
            }
        }
        const double stddev = std::sqrt(squaredDifferenceSum / static_cast<double>(count));
        return {mean, stddev};
    }

    template <typename T>
    void appendOptionalTextField(std::ostringstream &out, const char *label, const std::optional<T> &value)
    {
        if (value.has_value())
        {
            out << label << *value << "\n";
        }
    }

    template <typename T>
    void appendOptionalDimensions(std::ostringstream &out,
                                  const char *label,
                                  const std::optional<T> &width,
                                  const std::optional<T> &height)
    {
        if (width.has_value() && height.has_value())
        {
            out << label << *width << "x" << *height << "\n";
        }
    }

    std::string padText(const std::string &text, std::size_t width)
    {
        if (text.find("±") != std::string::npos)
        {
            ++width;
        }
        if (text.size() >= width)
        {
            return text.substr(0, width);
        }
        return text + std::string(width - text.size(), ' ');
    }

    std::string formatPerformance(const double mean,
                                  const double stddev,
                                  const char *unit,
                                  const int precision = 3)
    {
        const int meanWidth = (precision > 3) ? 12 : 9;
        const int stddevWidth = (precision > 3) ? 9 : 6;
        std::ostringstream out;
        out << std::fixed;
        out << std::setw(meanWidth) << std::setprecision(precision) << mean;
        out << " ± ";
        out << std::setw(stddevWidth) << std::setprecision(precision) << stddev;
        out << " (" << unit << ")";
        return out.str();
    }

    void appendPerformanceRow(std::ostringstream &out,
                              const std::string &framework,
                              const std::string &threads,
                              const std::string &test,
                              const double mean,
                              const double stddev,
                              const char *unit,
                              const int precision = 3)
    {
        out << "| " << padText(framework, frameworkColumnWidth)
            << " | " << padText(threads, threadsColumnWidth)
            << " | " << padText(test, testColumnWidth)
            << " | " << padText(formatPerformance(mean, stddev, unit, precision), performanceColumnWidth) << " |\n";
    }

    void appendOptionalPerformanceRow(std::ostringstream &out,
                                      const std::string &framework,
                                      const std::string &threads,
                                      const std::string &test,
                                      const std::optional<double> &mean,
                                      const std::optional<double> &stddev,
                                      const char *unit,
                                      const int precision = 3)
    {
        if (mean.has_value())
        {
            appendPerformanceRow(out,
                                 framework,
                                 threads,
                                 test,
                                 *mean,
                                 stddev.value_or(0.0),
                                 unit,
                                 precision);
        }
    }

    void appendTextParameters(std::ostringstream &out,
                              const BenchReport &report,
                              const std::string &modelPath,
                              const int contextSize,
                              const int numThreads,
                              const int numOutputTokens)
    {
        out << "\n=== ARM LLM Benchmark ===\n\n";
        out << "Parameters:\n";
        out << "  model_path         : " << modelPath << "\n";
        out << "  model_size         : " << FormatModelSize(report.modelSizeBytes) << "\n";

        if (report.workload.isScenario)
        {
            out << "  workload_type      : " << report.workload.workloadType << "\n";
            out << "  image_count        : " << report.workload.imageCount << "\n";
            if (report.workload.imageCount > 0)
            {
                const auto &imageMetadata = report.workload.imageMetadata;
                appendOptionalDimensions(out,
                                         "  original_image_size: ",
                                         imageMetadata.originalImageWidth,
                                         imageMetadata.originalImageHeight);
                appendOptionalDimensions(out,
                                         "  prepared_image_size: ",
                                         imageMetadata.preparedImageWidth,
                                         imageMetadata.preparedImageHeight);
                appendOptionalTextField(out, "  original_pixels    : ", imageMetadata.originalImagePixels);
                appendOptionalTextField(out, "  prepared_pixels    : ", imageMetadata.preparedImagePixels);
                appendOptionalTextField(out, "  processed_pixels   : ", imageMetadata.processedImagePixels);
                if (imageMetadata.processedImagePixels.has_value() &&
                    !imageMetadata.processedImagePixelsSource.empty())
                {
                    out << "  processed_source   : " << imageMetadata.processedImagePixelsSource << "\n";
                }
            }
            const auto &tokenMetadata = report.workload.tokenMetadata;
            appendOptionalTextField(out, "  text_prompt_tokens : ", tokenMetadata.textPromptTokens);
            if (report.workload.imageCount > 0)
            {
                appendOptionalTextField(out, "  vision_tokens      : ", tokenMetadata.visionTokens);
                appendOptionalTextField(out, "  fused_positions    : ", tokenMetadata.fusedPromptPositions);
            }
        }

        if (report.workload.inputTokens.has_value())
        {
            out << "  num_input_tokens   : " << *report.workload.inputTokens << "\n";
        }
        else
        {
            out << "  num_input_tokens   : unknown\n";
        }
        out << "  num_output_tokens  : " << numOutputTokens << "\n";
        out << "  context_size       : " << contextSize << "\n";
        out << "  num_threads        : " << numThreads << "\n";
        out << "  num_iterations     : " << report.config.measuredIterations << "\n";
        out << "  num_warmup         : " << report.config.warmupIterations << "\n\n";
    }

    void appendTextResults(std::ostringstream &out,
                           const BenchReport &report,
                           const std::string &frameworkType,
                           const int numThreads,
                           const int numInputTokens,
                           const int numOutputTokens)
    {
        const auto &mean = report.summary.mean;
        const auto &stddev = report.summary.stddev;
        const std::string threads = std::to_string(numThreads);

        out << "\n======= Results =========\n\n";
        out << "| " << padText("Framework", frameworkColumnWidth)
            << " | " << padText("Threads", threadsColumnWidth)
            << " | " << padText("Test", testColumnWidth)
            << " | " << padText("Performance", performanceColumnWidth) << " |\n";
        out << "| " << std::string(frameworkColumnWidth, '-')
            << " | " << std::string(threadsColumnWidth, '-')
            << " | " << std::string(testColumnWidth, '-')
            << " | " << std::string(performanceColumnWidth, '-') << " |\n";

        if (mean.encodeTokensPerSec.has_value())
        {
            const int inputTokens = report.workload.inputTokens.value_or(numInputTokens);
            appendPerformanceRow(out,
                                 frameworkType,
                                 threads,
                                 "pp" + std::to_string(inputTokens),
                                 *mean.encodeTokensPerSec,
                                 stddev.encodeTokensPerSec.value_or(0.0),
                                 "t/s");
        }
        else
        {
            appendPerformanceRow(out,
                                 frameworkType,
                                 threads,
                                 "Encode",
                                 mean.encodeTimeSec * 1000.0,
                                 stddev.encodeTimeSec * 1000.0,
                                 "ms");
        }
        appendOptionalPerformanceRow(out,
                                     frameworkType,
                                     threads,
                                     "prepMP",
                                     mean.preparedImageMegapixelsPerEncodeSec,
                                     stddev.preparedImageMegapixelsPerEncodeSec,
                                     "MP/s",
                                     6);
        appendOptionalPerformanceRow(out,
                                     frameworkType,
                                     threads,
                                     "visMP",
                                     mean.visionMegapixelsPerSec,
                                     stddev.visionMegapixelsPerSec,
                                     "MP/s",
                                     6);
        appendOptionalPerformanceRow(out,
                                     frameworkType,
                                     threads,
                                     "Vision",
                                     mean.visionTimeMs,
                                     stddev.visionTimeMs,
                                     "ms");
        appendOptionalPerformanceRow(out,
                                     frameworkType,
                                     threads,
                                     "Prefill",
                                     mean.prefillTimeMs,
                                     stddev.prefillTimeMs,
                                     "ms");
        appendPerformanceRow(out,
                             frameworkType,
                             threads,
                             "tg" + std::to_string(numOutputTokens),
                             mean.decodeTokensPerSec,
                             stddev.decodeTokensPerSec,
                             "t/s");
        if (report.workload.isScenario)
        {
            appendPerformanceRow(out,
                                 frameworkType,
                                 threads,
                                 "Decode",
                                 mean.decodeTimeSec * 1000.0,
                                 stddev.decodeTimeSec * 1000.0,
                                 "ms");
        }
        appendPerformanceRow(out,
                             frameworkType,
                             threads,
                             "TTFT",
                             mean.timeToFirstTokenMs,
                             stddev.timeToFirstTokenMs,
                             "ms");
        appendPerformanceRow(out,
                             frameworkType,
                             threads,
                             "Total",
                             mean.totalTimeMs,
                             stddev.totalTimeMs,
                             "ms");
    }

    template <typename T>
    void addOptionalJsonField(json &target, const char *key, const std::optional<T> &value)
    {
        if (value.has_value())
        {
            target[key] = *value;
        }
    }

    template <typename T, typename Formatter>
    void addOptionalJsonField(json &target,
                              const char *key,
                              const std::optional<T> &value,
                              Formatter formatter)
    {
        if (value.has_value())
        {
            target[key] = formatter(*value);
        }
    }

    template <typename T, typename Formatter>
    void addOptionalJsonStatistic(json &results,
                                  const char *key,
                                  const std::optional<T> &mean,
                                  const std::optional<T> &stddev,
                                  Formatter formatter)
    {
        if (mean.has_value())
        {
            results["mean"][key] = formatter(*mean);
            results["stddev"][key] = formatter(stddev.value_or(T{}));
        }
    }

} // namespace

BenchRunner::BenchRunner(IBenchAdapter &bench, const BenchRunConfig &config)
    : m_bench(bench), m_config(config)
{
}

int BenchRunner::Run(BenchReport &report) const
{
    if (m_config.measuredIterations <= 0)
    {
        LOG_ERROR("Measured iterations must be positive");
        return 1;
    }

    report = BenchReport{};
    report.config = m_config;
    report.modelSizeBytes = m_bench.GetModelSizeBytes();
    report.workload = m_bench.GetWorkloadMetadata();

    report.results.reserve(static_cast<size_t>(m_config.measuredIterations));
    if (m_config.warmupIterations > 0)
    {
        LOG_INF("Running %d warmup iteration(s) (results ignored)...", m_config.warmupIterations);
    }
    const int totalIterations = m_config.warmupIterations + m_config.measuredIterations;
    for (int iter = 0; iter < totalIterations; ++iter)
    {
        const bool isMeasured = (iter >= m_config.warmupIterations);
        const auto encode = m_bench.EncodeStep();
        BenchDecodeStepResult decode{};
        decode.decodeTimeSec = LlmBench::MeasureTimingSec(
            isMeasured ? "bench_runner.decode_total.measure" : "bench_runner.decode_total.warmup",
            [&]
            {
                for (int tokenIdx = 0; tokenIdx < m_bench.GetOutputTokens(); ++tokenIdx)
                {
                    const auto step = m_bench.DecodeStep();
                    decode.tokensGenerated += step.tokensGenerated;
                    if (tokenIdx == 0)
                    {
                        decode.firstTokenFromDecodeStartMs = step.firstTokenFromDecodeStartMs;
                    }
                }
            });
        m_bench.StopGeneration();
        const BenchIterationResult result = m_bench.BuildIterationResult(encode, decode);
        if (isMeasured)
        {
            report.results.push_back(result);
        }
        m_bench.FinishIteration();
    }

    report.workload = m_bench.GetWorkloadMetadata();
    report.summary = ComputeSummaryStats(report.results);
    return 0;
}

BenchSummaryStats BenchRunner::ComputeSummaryStats(const std::vector<BenchIterationResult> &results)
{
    BenchSummaryStats stats{};
    if (results.empty())
    {
        return stats;
    }

    const double n = static_cast<double>(results.size());
    for (const auto &ir : results)
    {
        stats.mean.timeToFirstTokenMs += ir.timeToFirstTokenMs;
        stats.mean.totalTimeMs += ir.totalTimeMs;
        stats.mean.encodeTimeSec += ir.encodeTimeSec;
        stats.mean.decodeTimeSec += ir.decodeTimeSec;
        stats.mean.decodeTokensPerSec += ir.decodeTokensPerSec;
    }
    stats.mean.timeToFirstTokenMs /= n;
    stats.mean.totalTimeMs /= n;
    stats.mean.encodeTimeSec /= n;
    stats.mean.decodeTimeSec /= n;
    stats.mean.decodeTokensPerSec /= n;

    for (const auto &ir : results)
    {
        stats.stddev.timeToFirstTokenMs += std::pow(ir.timeToFirstTokenMs - stats.mean.timeToFirstTokenMs, 2.0);
        stats.stddev.totalTimeMs += std::pow(ir.totalTimeMs - stats.mean.totalTimeMs, 2.0);
        stats.stddev.encodeTimeSec += std::pow(ir.encodeTimeSec - stats.mean.encodeTimeSec, 2.0);
        stats.stddev.decodeTimeSec += std::pow(ir.decodeTimeSec - stats.mean.decodeTimeSec, 2.0);
        stats.stddev.decodeTokensPerSec += std::pow(ir.decodeTokensPerSec - stats.mean.decodeTokensPerSec, 2.0);
    }
    stats.stddev.timeToFirstTokenMs = std::sqrt(stats.stddev.timeToFirstTokenMs / n);
    stats.stddev.totalTimeMs = std::sqrt(stats.stddev.totalTimeMs / n);
    stats.stddev.encodeTimeSec = std::sqrt(stats.stddev.encodeTimeSec / n);
    stats.stddev.decodeTimeSec = std::sqrt(stats.stddev.decodeTimeSec / n);
    stats.stddev.decodeTokensPerSec = std::sqrt(stats.stddev.decodeTokensPerSec / n);

    std::tie(stats.mean.encodeTokensPerSec, stats.stddev.encodeTokensPerSec) =
        calculateOptionalStats(results, [](const auto &result) { return result.encodeTokensPerSec; });
    std::tie(stats.mean.preparedImageMegapixelsPerEncodeSec,
             stats.stddev.preparedImageMegapixelsPerEncodeSec) =
        calculateOptionalStats(results,
                               [](const auto &result) { return result.preparedImageMegapixelsPerEncodeSec; });
    std::tie(stats.mean.visionMegapixelsPerSec, stats.stddev.visionMegapixelsPerSec) =
        calculateOptionalStats(results, [](const auto &result) { return result.visionMegapixelsPerSec; });
    std::tie(stats.mean.visionTimeMs, stats.stddev.visionTimeMs) =
        calculateOptionalStats(results, [](const auto &result) { return result.visionTimeMs; });
    std::tie(stats.mean.prefillTimeMs, stats.stddev.prefillTimeMs) =
        calculateOptionalStats(results, [](const auto &result) { return result.prefillTimeMs; });
    return stats;
}

std::string BenchRunner::FormatText(const BenchReport &report,
                                    const std::string &modelPath,
                                    const int contextSize,
                                    const int numThreads,
                                    const int numInputTokens,
                                    const int numOutputTokens,
                                    const std::string &frameworkType)
{
    std::ostringstream out;
    appendTextParameters(out,
                         report,
                         modelPath,
                         contextSize,
                         numThreads,
                         numOutputTokens);
    appendTextResults(out, report, frameworkType, numThreads, numInputTokens, numOutputTokens);
    return out.str();
}

std::string BenchRunner::FormatJson(const BenchReport &report,
                                    const std::string &modelPath,
                                    int contextSize,
                                    int numThreads,
                                    int numInputTokens,
                                    int numOutputTokens,
                                    const std::string &frameworkType)
{
    const auto roundTo = [](const double v, const double scale)
    {
        return std::round(v * scale) / scale;
    };
    const auto round3 = [&](const double v)
    {
        return roundTo(v, 1000.0);
    };
    const auto round6 = [&](const double v)
    {
        return roundTo(v, 1000000.0);
    };
    const std::string modelSize = FormatModelSize(report.modelSizeBytes);

    json out;
    out["parameters"] = {
        {"model_path", modelPath},
        {"model_size", modelSize},
        {"num_output_tokens", numOutputTokens},
        {"context_size", contextSize},
        {"num_threads", numThreads},
        {"num_iterations", report.config.measuredIterations},
        {"num_warmup", report.config.warmupIterations},
    };
    if (report.workload.isScenario)
    {
        out["parameters"]["workload_type"] = report.workload.workloadType;
        out["parameters"]["image_count"] = report.workload.imageCount;
        if (report.workload.imageCount > 0)
        {
            auto &parameters = out["parameters"];
            const auto &imageMetadata = report.workload.imageMetadata;
            addOptionalJsonField(parameters, "prepared_image_width", imageMetadata.preparedImageWidth);
            addOptionalJsonField(parameters, "prepared_image_height", imageMetadata.preparedImageHeight);
            addOptionalJsonField(parameters, "original_image_width", imageMetadata.originalImageWidth);
            addOptionalJsonField(parameters, "original_image_height", imageMetadata.originalImageHeight);
            addOptionalJsonField(parameters, "original_image_pixels", imageMetadata.originalImagePixels);
            addOptionalJsonField(parameters, "prepared_image_pixels", imageMetadata.preparedImagePixels);
            if (imageMetadata.processedImagePixels.has_value())
            {
                parameters["processed_image_pixels"] = imageMetadata.processedImagePixels.value();
                parameters["processed_image_pixels_source"] = imageMetadata.processedImagePixelsSource;
            }
        }
        const auto &tokenMetadata = report.workload.tokenMetadata;
        addOptionalJsonField(out["parameters"], "text_prompt_tokens", tokenMetadata.textPromptTokens);
        if (report.workload.imageCount > 0)
        {
            addOptionalJsonField(out["parameters"], "vision_tokens", tokenMetadata.visionTokens);
            addOptionalJsonField(out["parameters"], "fused_prompt_positions", tokenMetadata.fusedPromptPositions);
        }
    }
    if (report.workload.inputTokens.has_value())
    {
        out["parameters"]["num_input_tokens"] = report.workload.inputTokens.value();
    }
    else
    {
        out["parameters"]["num_input_tokens"] = nullptr;
    }
    out["framework"] = frameworkType;
    out["results"] = {
        {"mean", {
                     {"decode_tokens_per_sec", round3(report.summary.mean.decodeTokensPerSec)},
                     {"ttft_ms", round3(report.summary.mean.timeToFirstTokenMs)},
                     {"total_ms", round3(report.summary.mean.totalTimeMs)},
                 }},
        {"stddev", {
                       {"decode_tokens_per_sec", round3(report.summary.stddev.decodeTokensPerSec)},
                       {"ttft_ms", round3(report.summary.stddev.timeToFirstTokenMs)},
                       {"total_ms", round3(report.summary.stddev.totalTimeMs)},
                   }},
    };
    auto &results = out["results"];
    const auto &mean = report.summary.mean;
    const auto &stddev = report.summary.stddev;
    addOptionalJsonStatistic(results,
                             "encode_tokens_per_sec",
                             mean.encodeTokensPerSec,
                             stddev.encodeTokensPerSec,
                             round3);
    addOptionalJsonStatistic(results,
                             "prepared_image_megapixels_per_encode_sec",
                             mean.preparedImageMegapixelsPerEncodeSec,
                             stddev.preparedImageMegapixelsPerEncodeSec,
                             round6);
    addOptionalJsonStatistic(results,
                             "vision_megapixels_per_sec",
                             mean.visionMegapixelsPerSec,
                             stddev.visionMegapixelsPerSec,
                             round6);
    addOptionalJsonStatistic(results, "vision_time_ms", mean.visionTimeMs, stddev.visionTimeMs, round3);
    addOptionalJsonStatistic(results, "prefill_time_ms", mean.prefillTimeMs, stddev.prefillTimeMs, round3);
    if (report.workload.isScenario)
    {
        out["results"]["mean"]["encode_time_sec"] = round3(report.summary.mean.encodeTimeSec);
        out["results"]["mean"]["decode_time_sec"] = round3(report.summary.mean.decodeTimeSec);
        out["results"]["stddev"]["encode_time_sec"] = round3(report.summary.stddev.encodeTimeSec);
        out["results"]["stddev"]["decode_time_sec"] = round3(report.summary.stddev.decodeTimeSec);
    }

    out["iterations"] = json::array();
    for (const auto &ir : report.results)
    {
        json iteration = {
            {"time_to_first_token_ms", round3(ir.timeToFirstTokenMs)},
            {"total_time_ms", round3(ir.totalTimeMs)},
            {"tokens_generated", ir.tokensGenerated},
            {"encode_time_sec", round3(ir.encodeTimeSec)},
            {"decode_time_sec", round3(ir.decodeTimeSec)},
            {"decode_tokens_per_sec", round3(ir.decodeTokensPerSec)},
        };
        addOptionalJsonField(iteration, "encode_tokens_per_sec", ir.encodeTokensPerSec, round3);
        const auto &imageMetadata = ir.imageMetadata;
        addOptionalJsonField(iteration, "original_image_pixels", imageMetadata.originalImagePixels);
        addOptionalJsonField(iteration, "original_image_width", imageMetadata.originalImageWidth);
        addOptionalJsonField(iteration, "original_image_height", imageMetadata.originalImageHeight);
        addOptionalJsonField(iteration, "prepared_image_pixels", imageMetadata.preparedImagePixels);
        addOptionalJsonField(iteration, "prepared_image_width", imageMetadata.preparedImageWidth);
        addOptionalJsonField(iteration, "prepared_image_height", imageMetadata.preparedImageHeight);
        if (imageMetadata.processedImagePixels.has_value())
        {
            iteration["processed_image_pixels"] = imageMetadata.processedImagePixels.value();
            iteration["processed_image_pixels_source"] = imageMetadata.processedImagePixelsSource;
        }
        const auto &tokenMetadata = ir.tokenMetadata;
        addOptionalJsonField(iteration, "text_prompt_tokens", tokenMetadata.textPromptTokens);
        addOptionalJsonField(iteration, "vision_tokens", tokenMetadata.visionTokens);
        addOptionalJsonField(iteration, "fused_prompt_positions", tokenMetadata.fusedPromptPositions);
        addOptionalJsonField(iteration,
                             "prepared_image_megapixels_per_encode_sec",
                             ir.preparedImageMegapixelsPerEncodeSec,
                             round6);
        addOptionalJsonField(iteration, "vision_megapixels_per_sec", ir.visionMegapixelsPerSec, round6);
        addOptionalJsonField(iteration, "vision_time_ms", ir.visionTimeMs, round3);
        addOptionalJsonField(iteration, "prefill_time_ms", ir.prefillTimeMs, round3);
        out["iterations"].push_back(iteration);
    }

    return out.dump();
}
