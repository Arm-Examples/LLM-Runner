//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef LLM_BENCH_ADAPTER_HPP
#define LLM_BENCH_ADAPTER_HPP

#include "Llm.hpp"
#include "LlmConfig.hpp"
#include <cstdint>
#include <functional>
#include <string>

/**
 * @struct BenchIterationResult
 * @brief Aggregated benchmark metrics for one encode+decode iteration.
 *
 * Contains latency and throughput metrics computed from one measured
 * benchmark iteration, including time-to-first-token and total iteration time.
 */
struct BenchIterationResult {
    double timeToFirstTokenMs = 0.0; ///< Time from encode start to first decoded token.
    double totalTimeMs = 0.0;        ///< End-to-end iteration time (encode + decode loop).
    int tokensGenerated = 0;         ///< Number of tokens generated during decode.
    double encodeTimeSec = 0.0;      ///< Encode phase duration in seconds.
    double decodeTimeSec = 0.0;      ///< Decode loop duration in seconds.
    double encodeTokensPerSec = 0.0; ///< Effective encode throughput in tokens/second.
    double decodeTokensPerSec = 0.0; ///< Effective decode throughput in tokens/second.
};

/**
 * @struct BenchEncodeStepResult
 * @brief Timing result for a single encode phase.
 *
 * Represents the elapsed wall-clock time spent in one encode operation.
 */
struct BenchEncodeStepResult {
    double encodeTimeSec = 0.0; ///< Encode duration in seconds.
};

/**
 * @struct BenchDecodeStepResult
 * @brief Timing/counter result for one decode step or decode-step aggregate.
 *
 * Stores decode duration, generated token count, and first-token latency
 * measured from decode-loop start.
 */
struct BenchDecodeStepResult {
    int tokensGenerated = 0;                  ///< Number of generated tokens represented by this result.
    double decodeTimeSec = 0.0;               ///< Decode duration in seconds.
    double firstTokenFromDecodeStartMs = 0.0; ///< Time-to-first-token measured from decode start.
};

class IBenchAdapter {
public:
    virtual ~IBenchAdapter() = default;
    virtual BenchEncodeStepResult EncodeStep() = 0;
    virtual BenchDecodeStepResult DecodeStep() = 0;
    virtual BenchIterationResult BuildIterationResult(const BenchEncodeStepResult& encodeResult,
                                                      const BenchDecodeStepResult& decodeResult) const = 0;
    virtual void StopGeneration() = 0;
    virtual void FinishIteration() = 0;
    virtual int GetOutputTokens() const = 0;
    virtual uintmax_t GetModelSizeBytes() const = 0;
};

class LlmBench : public IBenchAdapter {
public:
    /**
     * @brief Construct a benchmark adapter around an initialized LLM API object.
     * @param llm LLM instance used to execute encode/decode operations.
     * @param numInputTokens Target prompt token count used for benchmark payload generation.
     * @param numOutputTokens Target decode token count per iteration.
     */
    LlmBench(LLM& llm, int numInputTokens, int numOutputTokens);

    /**
     * @brief Initialize the underlying LLM and prepare reusable benchmark payload state.
     * @param modelPath Path to model/config consumed by the selected backend.
     * @param numThreads Number of runtime threads.
     * @param contextSize Runtime context size in tokens.
     * @param sharedLibraryPath Directory used to resolve backend shared libraries.
     * @return 0 on success, non-zero on failure.
     */
    int Initialize(const std::string& modelPath, int numThreads, int contextSize, const std::string& sharedLibraryPath);
    /**
     * @brief Execute one encode step for the prepared payload.
     * @return Encode step timing result.
     */
    BenchEncodeStepResult EncodeStep() override;
    /**
     * @brief Execute one decode step (single token generation attempt).
     * @return Decode step timing result.
     */
    BenchDecodeStepResult DecodeStep() override;
    /**
     * @brief Build a full-iteration benchmark record from encode/decode step outputs.
     * @param encodeResult Encode step timings.
     * @param decodeResult Decode-step aggregate timings.
     * @return Iteration metrics including TTFT, total time, and throughput.
     */
    BenchIterationResult BuildIterationResult(const BenchEncodeStepResult& encodeResult,
                                              const BenchDecodeStepResult& decodeResult) const override;
    /**
     * @brief Build a full-iteration benchmark record from raw step timings and explicit metadata.
     * @param encodeResult Encode step timings.
     * @param decodeResult Decode-step aggregate timings.
     * @param numInputTokens Number of input tokens used to compute encode throughput.
     * @param frameworkType Framework/backend label available to downstream formatting or logging.
     * @return Iteration metrics including TTFT, total time, and throughput.
     */
    static BenchIterationResult BuildIterationResult(const BenchEncodeStepResult& encodeResult,
                                                     const BenchDecodeStepResult& decodeResult,
                                                     int numInputTokens,
                                                     const std::string& frameworkType);
    /**
     * @brief Request generation stop on the wrapped LLM.
     */
    void StopGeneration() override;
    /**
     * @brief Finalize an iteration by resetting context for the next iteration.
     */
    void FinishIteration() override;

    /**
     * @brief Return the configured benchmark input token count.
     */
    int GetInputTokens() const { return m_numInputTokens; }
    /**
     * @brief Return the configured benchmark output token count.
     */
    int GetOutputTokens() const override { return m_numOutputTokens; }
    /**
     * @brief Return the framework/backend type reported by the wrapped LLM.
     */
    std::string GetFrameworkType() const { return m_frameworkType; }
    /**
     * @brief Return the validated model package size in bytes.
     */
    uintmax_t GetModelSizeBytes() const override { return m_modelSizeBytes; }

    /**
     * @brief Measure wall-clock duration of an operation and optionally emit debug timing logs.
     * @param tag Log tag used in debug messages.
     * @param operation Callable to execute.
     * @return Elapsed duration in seconds.
     */
    static double MeasureTimingSec(const std::string& tag, const std::function<void()>& operation);

private:
    void PreparePayload();

    LLM& m_llm;
    int m_numInputTokens;
    int m_numOutputTokens;
    uintmax_t m_modelSizeBytes = 0;
    std::string m_frameworkType;
    LlmChat::Payload m_payload;
};

#endif /* LLM_BENCH_ADAPTER_HPP */
