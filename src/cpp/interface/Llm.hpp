//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "LlmConfig.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

/**
 * @class LLM
 * @brief Public interface for interacting with a Large Language Model (LLM).
 *
 * Thin wrapper that delegates to a concrete LLM implementation.
 */
class LLM {
public:

    /**
     * @struct EncodePayload
     * @brief Input payload for encoding a prompt.
     *
     * Encapsulates the parameters required when sending a prompt
     * (text, optional image, and conversation metadata) to the model.
     */
    struct EncodePayload {
        std::string textPrompt;     ///< Text query to encode
        std::string imagePath;      ///< Path to image (optional, leave empty if none)
        bool isFirstMessage{false}; ///< Whether this is the first conversation message
    };

    class LLMImpl; // Forward declaration for PImpl

    /**
     * @brief Construct an LLM instance with the given configuration.
     * @param llmConfig Configuration object specifying backend, model, and runtime options.
     */
    explicit LLM(const LlmConfig &llmConfig);
    ~LLM() noexcept;

    /**
     * @brief Deleted copy constructor.
     */
    LLM(const LLM&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    LLM& operator=(const LLM&) = delete;

    /**
     * @brief Move constructor.
     */
    LLM(LLM&&) noexcept = default;

    /**
     * @brief Move assignment operator.
     * @return Reference to this instance.
     */
    LLM& operator=(LLM&&) noexcept = default;

    /** Token that signifies the end of a response/generation. */
    inline static constexpr const char *endToken = "<eos>";

    /**
     * Initialize the underlying model.
     * @param llmConfig Model and user parameters.
     */
    void LlmInit(const LlmConfig &llmConfig);

    /** Free model resources. */
    void FreeLlm();

    /** @return Encode timings in milliseconds. */
    [[nodiscard]] float GetEncodeTimings() const;

    /** @return Decode timings in milliseconds. */
    [[nodiscard]] float GetDecodeTimings() const;

    /** Reset accumulated timings. */
    void ResetTimings();

    /** @return System information string. */
    [[nodiscard]] std::string SystemInfo() const;

    /**
     * Method to reset conversation history and preserve encoded system prompt.
     * If system prompt is not defined all conversation history would be cleared
     */
    void ResetContext();

    /**
     * Encode a text query into the model. Call NextToken() to retrieve tokens.
     * @param payload The input payload containing text and optional image data.
     */
    void Encode(EncodePayload& payload);

    /**
     * Retrieve the next token from the model after Encode().
     * @return A single token (possibly empty if generation has finished).
     */
    [[nodiscard]] std::string NextToken();

    /**
     * @return Percentage of context capacity used in the model cache.
     */
    [[nodiscard]] std::size_t GetChatProgress() const;

    /**
     * Benchmark the underlying backend.
     * @param nPrompts      Prompt length used for benchmarking.
     * @param nEvalPrompts  Number of generated tokens for benchmarking.
     * @param nMaxSeq       Maximum sequence length.
     * @param nRep          Number of repetitions.
     * @return Text report of prompt generation and evaluation results.
     */

    [[nodiscard]] std::string BenchModel(int &nPrompts, int &nEvalPrompts, int &nMaxSeq, int &nRep);

    /** @return Framework type string (e.g., backend name). */
    [[nodiscard]] std::string GetFrameworkType() const;

    /**
     * Format a prompt into a style the model understands for conversation.
     * @param prompt Raw prompt string (modified in-place).
     * @return Formatted query.
     */
    [[nodiscard]] std::string QueryBuilder(EncodePayload& prompt) const;

    /**
     * @return Vector of supported input modalities for the active implementation.
     */
    [[nodiscard]] std::vector<std::string> SupportedInputModalities() const;
    
    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */    
    void StopGeneration();

protected:
    std::unique_ptr<LLMImpl> m_impl{};                  /**< Implementation pointer. */

private:
    /**
     * Detect stop words in the internal token buffer and emit correct output.
     * @return Token string up to a stop word, end token, or partial output.
     */
    [[nodiscard]] std::string ContainsStopWord();

    LlmConfig m_config{};
    std::atomic<bool> m_evaluatedOnce{false};
    std::string m_streamEndFlag{};
    std::string m_tokenBuffer{};
    int m_maxStopWordLength{};
    bool m_stopFlag{false};
    bool SupportsModality(const std::vector<std::string> &inptMods, std::string modality) const;
};
