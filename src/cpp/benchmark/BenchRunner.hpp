//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef LLM_BENCH_RUNNER_HPP
#define LLM_BENCH_RUNNER_HPP

#include "LlmBench.hpp"
#include <string>
#include <vector>

/**
 * @struct BenchRunConfig
 * @brief Benchmark execution configuration used by BenchRunner.
 *
 * Defines warmup and measured iteration counts for a benchmark run.
 */
struct BenchRunConfig {
    int warmupIterations = 0;   ///< Warmup iterations executed before measurements (not included in report results).
    int measuredIterations = 1; ///< Number of measured iterations included in summary statistics.
};

/**
 * @struct BenchSummaryStats
 * @brief Summary statistics over measured benchmark iterations.
 *
 * Holds mean and standard deviation values computed from the per-iteration metrics.
 */
struct BenchSummaryStats {
    BenchIterationResult mean{};   ///< Mean values across measured iterations.
    BenchIterationResult stddev{}; ///< Population standard deviation across measured iterations.
};

/**
 * @struct BenchReport
 * @brief Complete benchmark output bundle.
 *
 * Contains run configuration, per-iteration measured results,
 * and computed summary statistics.
 */
struct BenchReport {
    BenchRunConfig config{};                     ///< Runner configuration used for this report.
    std::vector<BenchIterationResult> results{}; ///< Per-iteration measured benchmark records.
    BenchSummaryStats summary{};                 /// Aggregate summary statistics computed from results.
};

class BenchRunner {
public:
    /**
     * @brief Construct a benchmark runner over a benchmark adapter.
     * @param bench Benchmark adapter used for encode/decode operations.
     * @param config Warmup and measured iteration counts.
     */
    BenchRunner(IBenchAdapter& bench, const BenchRunConfig& config);

    /**
     * @brief Execute benchmark warmup and measured iterations.
     * @param report Output report populated with configuration, results, and summary.
     * @return 0 on success, non-zero on failure.
     */
    int Run(BenchReport& report);

    /**
     * @brief Format benchmark report as a human-readable table.
     * @param report Benchmark report containing configuration, iteration results, and summary metrics.
     * @param modelPath Path to the model or config used for the run.
     * @param contextSize Runtime context size in tokens.
     * @param numThreads Number of runtime threads used for the benchmark.
     * @param numInputTokens Number of input tokens encoded per iteration.
     * @param numOutputTokens Number of output tokens targeted per iteration.
     * @param frameworkType Backend/framework label reported with the results.
     * @return Formatted benchmark report text.
     */
    static std::string FormatText(const BenchReport& report,
                                  const std::string& modelPath,
                                  int contextSize,
                                  int numThreads,
                                  int numInputTokens,
                                  int numOutputTokens,
                                  const std::string& frameworkType);

    /**
     * @brief Format benchmark report as JSON.
     * @param report Benchmark report containing configuration, iteration results, and summary metrics.
     * @param modelPath Path to the model or config used for the run.
     * @param contextSize Runtime context size in tokens.
     * @param numThreads Number of runtime threads used for the benchmark.
     * @param numInputTokens Number of input tokens encoded per iteration.
     * @param numOutputTokens Number of output tokens targeted per iteration.
     * @param frameworkType Backend/framework label reported with the results.
     * @return Benchmark report serialized as JSON.
     */
    static std::string FormatJson(const BenchReport& report,
                                  const std::string& modelPath,
                                  int contextSize,
                                  int numThreads,
                                  int numInputTokens,
                                  int numOutputTokens,
                                  const std::string& frameworkType);

    /**
     * @brief Compute mean and standard deviation across measured iterations.
     * @param results Per-iteration measured benchmark results.
     * @return Aggregate mean and population standard deviation across the provided results.
     */
    static BenchSummaryStats ComputeSummaryStats(const std::vector<BenchIterationResult>& results);

private:
    IBenchAdapter& m_bench;
    BenchRunConfig m_config;
};

#endif /* LLM_BENCH_RUNNER_HPP */
