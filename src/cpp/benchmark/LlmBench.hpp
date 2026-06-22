//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef LLM_BENCH_ADAPTER_HPP
#define LLM_BENCH_ADAPTER_HPP

#include "BenchScenario.hpp"
#include "Llm.hpp"
#include "LlmConfig.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

/**
 * @struct BenchImageMetadata
 * @brief Image shape metadata observed for a benchmark payload.
 */
struct BenchImageMetadata
{
    std::optional<std::uint64_t> originalImagePixels{};  ///< Source image pixels before wrapper/backend preprocessing.
    std::optional<int> originalImageWidth{};             ///< Source image width before wrapper/backend preprocessing.
    std::optional<int> originalImageHeight{};            ///< Source image height before wrapper/backend preprocessing.
    std::optional<std::uint64_t> preparedImagePixels{};  ///< Wrapper-prepared image pixels passed to the backend.
    std::optional<int> preparedImageWidth{};             ///< Wrapper-prepared image width passed to the backend.
    std::optional<int> preparedImageHeight{};            ///< Wrapper-prepared image height passed to the backend.
    std::optional<std::uint64_t> processedImagePixels{}; ///< Backend-reported pixels at its documented preprocessing boundary.
    std::string processedImagePixelsSource{};            ///< Source of processedImagePixels, when present.
};

/**
 * @struct BenchTokenMetadata
 * @brief Backend token and position counts observed for a benchmark payload.
 */
struct BenchTokenMetadata
{
    std::optional<std::size_t> textPromptTokens{};     ///< Backend text prompt token count, when known.
    std::optional<std::size_t> visionTokens{};         ///< Backend vision token count, when known.
    std::optional<std::size_t> fusedPromptPositions{}; ///< Text plus vision positions consumed by prefill, when known.
};

/**
 * @struct BenchIterationResult
 * @brief Aggregated benchmark metrics for one encode+decode iteration.
 *
 * Contains latency and throughput metrics computed from one measured
 * benchmark iteration, including time-to-first-token and total iteration time.
 */
struct BenchIterationResult
{
    double timeToFirstTokenMs = 0.0;                        ///< Time from encode start to first decoded token.
    double totalTimeMs = 0.0;                               ///< End-to-end iteration time (encode + decode loop).
    int tokensGenerated = 0;                                ///< Number of tokens generated during decode.
    double encodeTimeSec = 0.0;                             ///< Encode phase duration in seconds.
    double decodeTimeSec = 0.0;                             ///< Decode loop duration in seconds.
    std::optional<int> inputTokens{};                       ///< Number of encoded input tokens, when known.
    std::optional<double> encodeTokensPerSec{};             ///< Effective encode throughput, when inputTokens is known.
    double decodeTokensPerSec = 0.0;                        ///< Effective decode throughput in generated tokens/second.
    BenchImageMetadata imageMetadata{};                     ///< Image metadata observed during this iteration.
    BenchTokenMetadata tokenMetadata{};                     ///< Token metadata observed during this iteration.
    std::optional<double> visionTimeMs{};                   ///< Backend vision stage time, when a trustworthy boundary is exposed.
    std::optional<double> prefillTimeMs{};                  ///< Backend prefill time, when exposed separately.
    std::optional<double> preparedImageMegapixelsPerEncodeSec{}; ///< Prepared image megapixels divided by full encode time.
    std::optional<double> visionMegapixelsPerSec{};         ///< Backend processed image megapixels divided by vision time.
};

/**
 * @struct BenchWorkloadMetadata
 * @brief Describes the stable benchmark payload shape for reporting.
 */
struct BenchWorkloadMetadata
{
    bool isScenario = false;            ///< Whether the benchmark payload came from `--scenario`.
    std::string workloadType = "text";  ///< Stable workload label used in reports.
    int imageCount = 0;                 ///< Number of images in the benchmark payload.
    BenchImageMetadata imageMetadata{}; ///< Stable image metadata for the configured payload.
    BenchTokenMetadata tokenMetadata{}; ///< Stable token metadata for the configured payload.
    std::optional<int> inputTokens{};   ///< Encoded input token count when available through the benchmark path.
};

/**
 * @struct BenchEncodeStepResult
 * @brief Timing result for a single encode phase.
 *
 * Represents the elapsed wall-clock time spent in one encode operation.
 */
struct BenchEncodeStepResult
{
    double encodeTimeSec = 0.0;           ///< Encode duration in seconds.
    LLM::InferenceStats inferenceStats{}; ///< Metrics captured by the wrapper/backend for the request.
};

/**
 * @struct BenchDecodeStepResult
 * @brief Timing/counter result for one decode step or decode-step aggregate.
 *
 * Stores decode duration, generated token count, and first-token latency
 * measured from decode-loop start.
 */
struct BenchDecodeStepResult
{
    int tokensGenerated = 0;                  ///< Number of generated tokens represented by this result.
    double decodeTimeSec = 0.0;               ///< Decode duration in seconds.
    double firstTokenFromDecodeStartMs = 0.0; ///< Time-to-first-token measured from decode start.
};

/**
 * @class IBenchAdapter
 * @brief Backend-independent benchmark operations consumed by BenchRunner.
 *
 * BenchRunner owns iteration control and reporting. Implementations of this
 * interface provide the model-specific encode/decode operations and metadata.
 */
class IBenchAdapter
{
public:
    /**
     * @brief Destroy the benchmark adapter.
     */
    virtual ~IBenchAdapter() = default;

    /**
     * @brief Execute and time one encode/prefill operation.
     * @return Encode timing information for the current iteration.
     */
    virtual BenchEncodeStepResult EncodeStep() = 0;

    /**
     * @brief Execute and time one decode token operation.
     * @return Decode timing and token-count information for the step.
     */
    virtual BenchDecodeStepResult DecodeStep() = 0;

    /**
     * @brief Convert encode/decode timings into one benchmark iteration result.
     * @param encodeResult Encode timing information.
     * @param decodeResult Aggregated decode timing information.
     * @return Full iteration metrics used by report formatting.
     */
    [[nodiscard]] virtual BenchIterationResult BuildIterationResult(const BenchEncodeStepResult &encodeResult,
                                                                    const BenchDecodeStepResult &decodeResult) const = 0;

    /**
     * @brief Request the wrapped model to stop any active generation.
     */
    virtual void StopGeneration() = 0;

    /**
     * @brief Reset or finalize adapter state after one benchmark iteration.
     */
    virtual void FinishIteration() = 0;

    /**
     * @brief Return the configured number of decode tokens per iteration.
     */
    [[nodiscard]] virtual int GetOutputTokens() const = 0;

    /**
     * @brief Return the validated size of the model artifacts being benchmarked.
     */
    [[nodiscard]] virtual uintmax_t GetModelSizeBytes() const = 0;

    /**
     * @brief Return metadata describing the benchmark workload payload.
     */
    [[nodiscard]] virtual BenchWorkloadMetadata GetWorkloadMetadata() const = 0;
};

class LlmBench : public IBenchAdapter
{
public:
    /**
     * @brief Construct a benchmark adapter around an initialized LLM API object.
     * @param llm LLM instance used to execute encode/decode operations.
     * @param numInputTokens Target prompt token count used for benchmark payload generation.
     * @param numOutputTokens Target decode token count per iteration.
     */
    LlmBench(LLM &llm, int numInputTokens, int numOutputTokens);

    /**
     * @brief Construct a scenario benchmark adapter around an initialized LLM API object.
     * @param llm LLM instance used to execute encode/decode operations.
     * @param numInputTokens Optional input token count retained for compatibility; scenario payloads do not use it.
     * @param numOutputTokens Target decode token count per iteration.
     * @param scenario Scenario payload loaded from JSON.
     */
    LlmBench(LLM &llm, int numInputTokens, int numOutputTokens, const BenchScenario &scenario);

    /**
     * @brief Initialize the underlying LLM and prepare reusable benchmark payload state.
     * @param modelPath Path to model/config consumed by the selected backend.
     * @param numThreads Number of runtime threads.
     * @param contextSize Runtime context size in tokens.
     * @param sharedLibraryPath Directory used to resolve backend shared libraries.
     * @param modelRootPath Optional root used to resolve relative paths in model config JSONs.
     * @return 0 on success, non-zero on failure.
     */
    int Initialize(const std::string &modelPath,
                   int numThreads,
                   int contextSize,
                   const std::string &sharedLibraryPath,
                   const std::string &modelRootPath = "");
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
    [[nodiscard]] BenchIterationResult BuildIterationResult(const BenchEncodeStepResult &encodeResult,
                                                            const BenchDecodeStepResult &decodeResult) const override;
    /**
     * @brief Build a full-iteration benchmark record from raw step timings and explicit metadata.
     * @param encodeResult Encode step timings.
     * @param decodeResult Decode-step aggregate timings.
     * @param numInputTokens Number of input tokens used to compute encode throughput.
     * @return Iteration metrics including TTFT, total time, and throughput.
     */
    static BenchIterationResult BuildIterationResult(const BenchEncodeStepResult &encodeResult,
                                                     const BenchDecodeStepResult &decodeResult,
                                                     int numInputTokens);
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
    [[nodiscard]] int GetInputTokens() const { return m_numInputTokens; }
    /**
     * @brief Return the configured benchmark output token count.
     */
    [[nodiscard]] int GetOutputTokens() const override { return m_numOutputTokens; }
    /**
     * @brief Return the framework/backend type reported by the wrapped LLM.
     */
    [[nodiscard]] std::string GetFrameworkType() const { return m_frameworkType; }
    /**
     * @brief Return the validated model package size in bytes.
     */
    [[nodiscard]] uintmax_t GetModelSizeBytes() const override { return m_modelSizeBytes; }
    /**
     * @brief Return workload metadata used by benchmark reporting.
     */
    [[nodiscard]] BenchWorkloadMetadata GetWorkloadMetadata() const override { return m_workloadMetadata; }

    /**
     * @brief Measure wall-clock duration of an operation and optionally emit debug timing logs.
     * @param tag Log tag used in debug messages.
     * @param operation Callable to execute.
     * @return Elapsed duration in seconds.
     */
    static double MeasureTimingSec(const std::string &tag, const std::function<void()> &operation);

private:
    enum class PayloadKind
    {
        SyntheticText,
        ScenarioText,
        ScenarioImage,
    };

    static PayloadKind ResolvePayloadKind(const std::optional<BenchScenario> &scenario);

    [[nodiscard]] bool IsScenarioRun() const;
    [[nodiscard]] bool IsVisionRun() const;
    [[nodiscard]] bool UsesSyntheticPrompt() const;

    void ValidateInitializeArgs(const std::string &modelPath, int numThreads, int contextSize) const;
    LlmConfig BuildBenchmarkConfig(const std::string &modelPath, const std::string &modelRootPath);
    static void ApplyRuntimeConfig(LlmConfig &config, int numThreads, int contextSize);
    void ConfigureScenarioWorkloadMetadata(const BenchScenario &scenario);

    /**
     * @brief Prepare the reusable base payload for the resolved benchmark payload mode.
     */
    void PreparePayload();

    /**
     * @brief Record metadata produced by a completed encode step.
     * @param result Encode step result to update.
     */
    void RecordEncodeMetadata(BenchEncodeStepResult &result);

    LLM &m_llm;                                             ///< Wrapped public LLM API instance.
    int m_numInputTokens;                                   ///< Requested synthetic input token count for text mode.
    int m_numOutputTokens;                                  ///< Requested decode token count per iteration.
    uintmax_t m_modelSizeBytes = 0;                         ///< Validated model package size in bytes.
    std::string m_frameworkType;                            ///< Framework/backend label reported by the LLM wrapper.
    LlmChat::Payload m_payload;                             ///< Base payload copied for each encode iteration.
    PayloadKind m_payloadKind = PayloadKind::SyntheticText; ///< Resolved benchmark payload mode.
    std::optional<BenchScenario> m_scenario;                ///< Optional user-provided scenario payload.
    BenchWorkloadMetadata m_workloadMetadata{};             ///< Scenario/text metadata used for reporting.
    bool m_hasObservedWorkloadMetadata = false;             ///< True after stable request metadata is captured.
};

#endif /* LLM_BENCH_ADAPTER_HPP */
