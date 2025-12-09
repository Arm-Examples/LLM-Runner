//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef LLM_BENCH_HPP
#define LLM_BENCH_HPP

#include "Llm.hpp"
#include "LlmConfig.hpp"
#include <string>
#include <vector>

/**
 * @class LlmBenchmark
 * @brief Lightweight benchmarking wrapper around the LLM public API.
 *
 * LlmBenchmark owns a minimal LlmConfig, initializes the LLM,
 * generates a synthetic input prompt with a fixed token length,
 * and runs encode/decode loops to measure per-iteration timings.
 *
 * It is intended to be used by the standalone arm-llm-bench binary
 * and is framework-agnostic: each backend provides its own
 * GeneratePromptWithNumTokens implementation under the LLM API.
 */
class LlmBenchmark : public LLM {
public:
    /**
     * @struct IterationResult
     * @brief Per-iteration timing and throughput statistics.
     *
     * Captures encode/decode timing and derived metrics such as
     * tokens-per-second for a single benchmark iteration.
     */
    struct IterationResult {
        double timeToFirstTokenMs   = 0.0; ///< Time to first generated token (TTFT) in milliseconds
        double totalTimeMs          = 0.0; ///< Total iteration time in milliseconds (encode + decode)
        int    tokensGenerated      = 0;   ///< Number of tokens generated during the decode phase
        double encodeTimeSec        = 0.0; ///< Encode phase duration in seconds
        double decodeTimeSec        = 0.0; ///< Decode phase duration in seconds
        double encodeTokensPerSec   = 0.0; ///< Throughput of encode phase in tokens per second
        double decodeTokensPerSec   = 0.0; ///< Throughput of decode phase in tokens per second
    };

    /**
     * Constructs a benchmark runner with the given parameters.
     *
     * @param modelPath             ///< Path to the model configuration or model file.
     * @param numInputTokens        ///< Target number of input tokens for the synthetic prompt.
     * @param numOutputTokens       ///< Number of output tokens to generate per iteration.
     * @param numThreads            ///< Number of runtime threads to configure in the LLM.
     * @param numIterations         ///< Number of measured benchmark iterations to run.
     * @param numWarmupIterations   ///< Number of warm-up iterations to run (ignored in stats).
     * @param sharedLibraryPath     ///< Shared library path.
     */
    LlmBenchmark(const std::string& modelPath,
                 int numInputTokens,
                 int numOutputTokens,
                 int numThreads,
                 int numIterations = 5,
                 int numWarmupIterations = 1,
                 const std::string& sharedLibraryPath="");
    /**
     * Default deconstructor
     */
    ~LlmBenchmark() noexcept override;

    /**
     * Runs the full benchmark sequence.
     *
     * Steps:
     *  - Initialize config and LLM
     *  - Generate a synthetic prompt with fixed numInputTokens tokens
     *  - Run warm-up iterations (ignored in statistics)
     *  - Run measured iterations and collect IterationResult entries
     *
     * @return 0 on success, non-zero on failure.
     */
    int Run();

    /**
     * Return the last benchmark iteration results as a formatted string.
     * @return Multi-line string summarizing performance results.
     */
    std::string GetResults() const;

private:
    // User-provided benchmark parameters
    std::string m_modelPath;                ///< Path to model config / model file
    int m_numInputTokens;                   ///< Input prompt size in tokens
    int m_numOutputTokens;                  ///< Number of output tokens to generate
    int m_numThreads;                       ///< Number of runtime threads
    int m_numIterations;                    ///< Number of measured benchmark iterations
    int m_numWarmupIterations;              ///< Number of warm-up iterations (ignored in stats)
    std::string m_sharedLibraryPath;        ///< Shared library path

    // Internal state
    LlmConfig m_config;                     ///< Local configuration used to initialize the LLM
    LlmChat::Payload m_payload;             ///< Synthetic payload reused across iterations
    std::string m_modelName;                ///< Resolved model name/path for logging
    std::string m_frameworkType;            ///< Backend framework identifier (llama.cpp, MNN, etc.)
    std::vector<IterationResult> m_results; ///< Iteration results

    /**
     * Initializes or adjusts configuration parameters.
     *
     * Currently most configuration is handled in the constructor,
     * but this hook is kept for future extensions (e.g. reading
     * additional runtime options).
     */
    void InitConfig();

     /**
      * Initializes the LLM instance using the stored configuration.
      *
      * Calls LlmInit() on the base LLM, and queries the framework type
      * for logging.
      */
    void InitLlm();

    /**
     * Prepares the synthetic benchmark payload.
     *
     * Uses LLM::GeneratePromptWithNumTokens to build a text prompt
     * that tokenizes to m_numInputTokens tokens under the active
     * backend, and stores it in m_payload.
     */
    void PreparePayload();

    /**
     * Runs a single benchmark iteration.
     *
     * Performs:
     *  - Encode() timing
     *  - NextToken() loop for m_numOutputTokens tokens
     *  - TTFT and total latency measurement
     *  - Encode/decode tokens-per-second calculations
     *
     * Resets the LLM context at the end of the iteration.
     *
     * @param iterationIndex Index of the current iteration (for logging).
     * @return IterationResult with timings and throughput metrics.
     */
    IterationResult RunSingleIteration(int iterationIndex);
};

#endif /* LLM_BENCH_HPP */
