//
// SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "LlmConfig.hpp"
#include "LlmChat.hpp"
#include <cstdint>
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
    class LLMImpl; // Forward declaration for PImpl

    /**
     * @brief Construct an LLM instance.
     */
    explicit LLM();
    virtual ~LLM() noexcept;

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
     * @param sharedLibraryPath Specify the location of optional shared libraries.
     */
    void LlmInit(const LlmConfig &llmConfig, std::string sharedLibraryPath = "");

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
    void Encode(LlmChat::Payload& payload);

    /**
     * Retrieve the next token from the model after Encode().
     * @return A single token (possibly empty if generation has finished).
     */
    [[nodiscard]] std::string NextToken();

    /** 
     * Function to produce next token
     * @param operationId can be used to return an error or check for user cancel operation requests
     * @return the next Token for Encoded Prompt
     */
    std::string CancellableNextToken(long operationId) const;

    /**
     * Function to request the cancellation of a ongoing operation / functional call
     * @param operationId associated with operation / functional call
     */
    void Cancel(long operationId);

    /**
     * @return Percentage of context capacity used in the model cache.
     */
    [[nodiscard]] std::size_t GetChatProgress() const;

    /** @return Framework type string (e.g., backend name). */
    [[nodiscard]] static std::string GetFrameworkType();

    /**
     * @return Vector of supported input modalities for the active implementation.
     */
    [[nodiscard]] std::vector<std::string> SupportedInputModalities() const;

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration();

    /**
     * Creates a synthetic text prompt that tokenizes to the given size.
     * @param numPromptTokens Desired number of input tokens.
     * @return A text prompt that produces that many tokens when encoded.
     */
    std::string GeneratePromptWithNumTokens(size_t numPromptTokens);

protected:
    std::unique_ptr<LLMImpl> m_impl{};                  /**< Implementation pointer. */

private:
    /**
     * Checks token to see if its a stop token
     * @param token checks token
     * @return return true if it is a stop token.
    */
    [[nodiscard]] bool isStopToken(std::string token);

    LlmConfig m_config{};
    bool SupportsModality(const std::vector<std::string> &inptMods, std::string modality) const;

};
