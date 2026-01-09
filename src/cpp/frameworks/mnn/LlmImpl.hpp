//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef LLM_IMPL_HPP
#define LLM_IMPL_HPP

#include "Llm.hpp"
#include "LlmConfig.hpp"
#include "LlmChat.hpp"

#include "llm/llm.hpp"
#include <MNN/AutoTime.hpp>
#include <MNN/expr/ExecutorScope.hpp>

/* Forward declaration */
class LLM;

/**
 * @brief MNN Implementation of our LLM API
 */
class LLM::LLMImpl : public LlmChat {

public:
    LLMImpl();
    ~LLMImpl();

    /**
     * Method to initialize a MNN model
     * @param config Configuration class with model's parameter and user defined parameters
     * @param sharedLibraryPath path to location of shared libs
     */
    void LlmInit(const LlmConfig& config, std::string sharedLibraryPath);

    /**
     * Method to free all allocations pertaining to MNN model
     */
    void FreeLlm();

    /**
     * Function to retrieve the MNN encode timings.
     * @return The encoded tokens per second
     */
    float GetEncodeTimings();

    /**
     * Function to retrieve the MNN decode timings.
     * @return The decoded tokens per second
     */
    float GetDecodeTimings();

    /**
     * Function to reset the MNN timing
     */
    void ResetTimings();

    /**
     * Function to print the system info
     * @return System info as a char pointer
     */
    std::string SystemInfo();

    /**
     * Method to reset conversation history and preserve model's character prefix.
     * If model's prefix is not defined all conversation history would be cleared
     */
    void ResetContext();

    /**
     * Encode a multimodal payload (text + optional image).
     * @param payload Input payload containing text and/or image path.
     */
    void Encode(LlmChat::Payload& payload);

    /**
     * Method to produce next token
     * @return the next token for encoded prompt
     */
    std::string NextToken();

    /**
    * Method to request the cancellation of an ongoing operation / functional call
    */
    void Cancel();

    /**
     * The method return the percentage of chat context filled
     * @return chat capacity filled in cache as percentage number
     */
    size_t GetChatProgress();

    /**
     * Method to get framework type
     * @return string framework type
     */
    static std::string GetFrameworkType() {return "mnn";}

    /**
     * @brief List supported input modalities.
     * @return A vector containing {"text", "vision"}.
     */
    std::vector<std::string> SupportedInputModalities() const;

    /**
     * Applies the automatic chat template to the given prompt.
     * @param payload The input prompt to apply the template to.
     * @return The prompt with the automatic chat template applied.
     */
    bool ApplyAutoChatTemplate(LlmChat::Payload& payload) override;

    /**
     * Applies image path embedding into text prompt
     * @param payload The input payload containing the user's text prompt and the image path to apply the template to.
     */
    void QueryBuilder(LlmChat::Payload& payload) override {
        if(payload.imagePath != "") {
            payload.textPrompt = "<img><hw>" +
                                std::to_string(IMG_HEIGHT) + ", " +
                                std::to_string(IMG_WIDTH) + "</hw>" +
                                payload.imagePath + "</img>" +
                                payload.textPrompt;
        }
        LlmChat::QueryBuilder(payload);
    }

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration();

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
    // Max image width
    const int IMG_WIDTH = 128;
    // Max image height
    const int IMG_HEIGHT = 128;
    // Model pointer
    std::unique_ptr<MNN::Transformer::Llm> m_llm = nullptr;
    // Context pointer
    const MNN::Transformer::LlmContext* m_ctx = nullptr;
    // Number of threads to use for model inference.
    size_t m_numOfThreads{0};
    // Maximum context length (number of tokens) supported by the model.
    int m_nCtx{0};
    // Batch size for token generation operations.
    size_t m_batchSz{0};
    // Filesystem path to the ONNX model.
    std::string m_modelPath{""};
    // Configuration for model
    LlmConfig m_config;
    // Used as a general signal in our LLM module to terminate response
    std::string m_eos = "<|endoftext|>";

    // Function to set the configurations
    void SetConfig();

};

#endif /* LLM_IMPL_HPP */
