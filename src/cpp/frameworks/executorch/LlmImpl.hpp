//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_IMPL_HPP
#define LLM_IMPL_HPP

#include <memory>
#include <string>
#include <vector>

#include "Llm.hpp"
#include "LlmConfig.hpp"
#include "LlmChat.hpp"

class LLM;
namespace tokenizers {
class Tokenizer;
}

/**
 * @brief ExecuTorch implementation for the LLM API.
 */
class LLM::LLMImpl : public LlmChat {
public:
    LLMImpl();
    ~LLMImpl();

    /**
     * Method to initialize an ExecuTorch model
     * @param config Configuration class with model's parameter and user defined parameters
     * @param sharedLibraryPath path to location of shared libs
     */
    void LlmInit(const LlmConfig& config, std::string sharedLibraryPath);

    /**
     * Method to free all allocations pertaining to ExecuTorch model
     */
    void FreeLlm();

    /**
     * Function to retrieve the ExecuTorch encode timings.
     * @return The encoded tokens per second
     */
    float GetEncodeTimings();

    /**
     * Function to retrieve the ExecuTorch decode timings.
     * @return The decoded tokens per second
     */
    float GetDecodeTimings();

    /**
     * Function to reset the ExecuTorch timing
     */
    void ResetTimings();

    /**
     * Function to print the system info
     * @return System info as a char pointer
     */
    std::string SystemInfo();

    /**
     * Method to reset the whole conversation history
     */
    void ResetContext();

    /**
     * Encode a payload containing text.
     * @param payload Input payload containing text.
     */
    void Encode(LlmChat::Payload& payload);

    /**
     * Method to produce next token
     * @return the next token for encoded prompt
     */
    std::string NextToken();

    /**
    * Method to request the cancellation of a ongoing operation / functional call
    */
    void Cancel();

    /**
     * The method return the percentage of chat context filled
     * @return chat capacity filled in cache as percentage number
     */
    size_t GetChatProgress() const;

    /**
     * Method to get framework type
     * @return string framework type
     */
    static std::string GetFrameworkType() { return "executorch"; }

    /**
     * @brief List supported input modalities.
     * @return A vector containing {"text"}.
     */
    std::vector<std::string> SupportedInputModalities() const { return {"text"}; }

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration();

    /**
     * Applies the automatic chat template to the given prompt.
     * @param payload The input prompt to apply the template to.
     * @return The prompt with the automatic chat template applied.
     */
    bool ApplyAutoChatTemplate(LlmChat::Payload& payload) override;

    /**
     * @brief Creates a synthetic text prompt that tokenizes to the given size.
     *
     * Used for benchmarking to ensure the encode phase receives a fixed
     * number of input tokens.
     *
     * @param numPromptTokens Desired number of input tokens.
     * @return A text prompt that produces that many tokens when encoded.
     */
    std::string GeneratePromptWithNumTokens(size_t numPromptTokens);

private:
    class SynchronousTextRunner;

    void EnsureInitialized(const char* operation) const;
    void ConfigureThreadPool();
    void WarnIfContextSizeExceedsModelLimit(tokenizers::Tokenizer* tokenizer) const;
    std::string ResolveTokenizerPath() const;

    LlmConfig m_config{};
    std::string m_modelPath{};
    std::string m_tokenizerPath{};
    std::string m_sharedLibraryPath{};
    int m_nCtx{0};
    int m_numThreads{0};
    size_t m_contextFilled{0};
    size_t m_totalDecodedTokens{0};
    size_t m_totalEncodedTokens{0};
    double m_totalDecoderTime{0.0};
    double m_totalEncoderTime{0.0};
    bool m_initialized{false};
    std::string m_eos{LLM::endToken};

    std::unique_ptr<SynchronousTextRunner> m_runner{nullptr};
    std::unique_ptr<tokenizers::Tokenizer> m_promptTokenizer{nullptr};
};

#endif /* LLM_IMPL_HPP */
