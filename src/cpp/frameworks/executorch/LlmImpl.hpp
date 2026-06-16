//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef EXECUTORCH_LLM_IMPL_HPP
#define EXECUTORCH_LLM_IMPL_HPP

#include <memory>
#include <string>
#include <vector>

#include "interface/LlmImpl.hpp"
#include "LlmConfig.hpp"
#include "LlmChat.hpp"

namespace tokenizers {
class Tokenizer;
}

/**
 * @brief ExecuTorch implementation for the LLM API.
 */
class ExecuTorchImpl : public LLM::LLMImpl {
public:
    ExecuTorchImpl();
    ~ExecuTorchImpl() override;

    /**
     * Method to initialize an ExecuTorch model
     * @param config Configuration class with model's parameter and user defined parameters
     * @param sharedLibraryPath path to location of shared libs
     */
    void LlmInit(const LlmConfig& config, std::string sharedLibraryPath) override;

    /**
     * Method to free all allocations pertaining to ExecuTorch model
     */
    void FreeLlm() override;

    /**
     * Function to retrieve the ExecuTorch encode timings.
     * @return The encoded tokens per second
     */
    float GetEncodeTimings() override;

    /**
     * Function to retrieve the ExecuTorch decode timings.
     * @return The decoded tokens per second
     */
    float GetDecodeTimings() override;

    /**
     * Function to reset the ExecuTorch timing
     */
    void ResetTimings() override;

    /**
     * Function to print the system info
     * @return System info as a char pointer
     */
    std::string SystemInfo() override;

    /**
     * Method to reset the whole conversation history
     */
    void ResetContext() override;

    /**
     * Encode a payload containing text.
     * @param payload Input payload containing text.
     */
    void Encode(LlmChat::Payload& payload) override;

    /**
     * Method to produce next token
     * @return the next token for encoded prompt
     */
    std::string NextToken() override;

    /**
    * Method to request the cancellation of a ongoing operation / functional call
    */
    void Cancel() override;

    /**
     * The method return the percentage of chat context filled
     * @return chat capacity filled in cache as percentage number
     */
    size_t GetChatProgress() const override;

    /**
     * Method to get framework type
     * @return string framework type
     */
    std::string GetFrameworkType() const override { return "executorch"; }

    /**
     * @brief List supported input modalities.
     * @return A vector containing {"text"}.
     */
    std::vector<std::string> SupportedInputModalities() const override { return {"text"}; }

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration() override;

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
    std::string GeneratePromptWithNumTokens(size_t numPromptTokens) override;

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

#endif /* EXECUTORCH_LLM_IMPL_HPP */
