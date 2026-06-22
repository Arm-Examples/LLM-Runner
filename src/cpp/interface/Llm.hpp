//
// SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "LlmConfig.hpp"
#include "LlmChat.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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

    /** Canonical reasons why generation stopped. */
    enum class TerminationReason {
        None,
        BackendEos,
        StopWord,
        ContextFull,
        Cancelled
    };

    /** Public text token id type. */
    using TextTokenId = int32_t;

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
     * Encode a text query into the model. Call NextTokenId() to retrieve token ids.
     * @param payload The input payload containing text and optional image data.
     */
    void Encode(LlmChat::Payload& payload);

    /** @return The next generated token id, or no value when generation stops. */
    [[nodiscard]] std::optional<TextTokenId> NextTokenId();

    /** @return The next generated token id, or no value when generation stops or is cancelled. */
    std::optional<TextTokenId> CancellableNextTokenId(long operationId) const;

    /** @return The decoded text for a token id. */
    std::string DetokenizeTextToken(TextTokenId token);

    /**
     * @return The reason the most recent token-id query returned no token.
     *
     * This is currently used to distinguish cancellation from normal completion
     * after the token-return API was simplified to `std::optional<TextTokenId>`.
     */
    [[nodiscard]] TerminationReason GetLastTerminationReason() const;

    /**
     * @brief Check whether a decoded text fragment matches one of the configured stop words.
     * @param text Decoded text piece.
     * @return true when `text` should terminate the decoded stream.
     */
    [[nodiscard]] bool IsStopTextPiece(const std::string& text) const;

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
    LlmConfig m_config{};
    bool SupportsModality(const std::vector<std::string> &inptMods, std::string modality) const;

};
