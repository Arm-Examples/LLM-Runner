//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_IMPL_HPP
#define LLM_IMPL_HPP

#include <unordered_map>

#include "Llm.hpp"
#include "LlmConfig.hpp"
#include "LlmBridge.hpp"

#include "ort_genai.h"

/* Forward declaration */
class LLM;

/**
 * @brief ONNX Implementation of our LLM API
 */
class LLM::LLMImpl : public LlmChat {

public:
    LLMImpl();
    ~LLMImpl();

    /**
     * Method to initialize a ONNX model
     * @param config Configuration class with model's parameter and user defined parameters
     * @param sharedLibraryPath path to location of shared libs
     */
    void LlmInit(const LlmConfig& config, std::string sharedLibraryPath);

    /**
     * Method to free all allocations pertaining to ONNX model
     */
    void FreeLlm();

    /**
     * Function to retrieve the ONNX encode timings.
     * @return The encoded tokens per second
     */
    float GetEncodeTimings();

    /**
     * Function to retrieve the ONNX decode timings.
     * @return The decoded tokens per second
     */
    float GetDecodeTimings();

    /**
     * Function to reset the ONNX timing
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
    static std::string GetFrameworkType() {return "onnxruntime-genai";}

    /**
     * @brief List supported input modalities.
     * @return A vector containing {"text", "vision"}.
     */
    std::vector<std::string> SupportedInputModalities() const{  return {"text"};}

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration();

    /**
     * Applies the automatic chat template to the given prompt.
     * @param prompt The input prompt to apply the template to.
     * @return The prompt with the automatic chat template applied.
     */
    bool ApplyAutoChatTemplate(LlmChat::Payload& prompt) override;

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
    // Pointer to the loaded OgaModel used for inference
    std::unique_ptr<OgaModel> m_llmModelPtr {nullptr};
    // Pointer to the OgaConfig instance containing model configuration settings.
    std::unique_ptr<OgaConfig> m_llmConfigsPtr {nullptr};
    // Pointer to the OgaGeneratorParams instance holding generation parameters.
    std::unique_ptr<OgaGeneratorParams> m_llmGntParamsPtr {nullptr};
    // Pointer to the OgaGenerator object responsible for text generation.
    std::unique_ptr<OgaGenerator> m_llmGeneratorPtr {nullptr};
    // Pointer to the OgaTokenizer used for tokenizing input text.
    std::unique_ptr<OgaTokenizer> m_tokenizerPtr {nullptr};
    // Pointer to the OgaTokenizerStream used for streaming tokenized outputs.
    std::unique_ptr<OgaTokenizerStream> m_tokenizerStreamPtr {nullptr};
    // Pointer to the OgaSequences container storing generated token sequences.
    std::unique_ptr<OgaSequences> m_sequencesPtr {nullptr};

    // Number of threads to use for model inference.
    size_t m_numOfThreads{0};
    // Maximum context length (number of tokens) supported by the model.
    int m_nCtx{0};
    // length of context filled in absolute token number
    size_t m_nCurr{0};
    // Batch size for token generation operations.
    size_t m_batchSz{0};
    // Filesystem path to the ONNX model.
    std::string m_modelPath{""};
    // Indicates whether the LLM has been initialized.
    bool m_llmInitialized{false};
    // Proportion of the context window currently filled (as % of total tokens)
    size_t m_contextFilled{0};
    // Total number of decoded tokens
    size_t m_totalDecodedTokens{0};
    // Total number of encoded tokens
    size_t m_totalEncodedTokens{0};
    // Total time for decoder
    double m_totalDecoderTime{0.0};
    // Total time for encoder
    double m_totalEncoderTime = {0.0};
    // Configuration for model
    LlmConfig m_config;


    // Map llm-log-level to Onnx log severity level
    std::unordered_map<int,int> m_onnxLogMap{
        {/*LLM_LOG_ERROR*/   0 , /*ORT_LOGGING_LEVEL_ERROR*/ 3},
        {/*LLM_LOG_WARN*/    3 ,/*ORT_LOGGING_LEVEL_WARN*/   2},
        {/*LLM_LOG_INFO*/    2 ,/*ORT_LOGGING_LEVEL_INFO*/   1},
        {/*LLM_LOG_DEBUG*/   3 ,/*ORT_LOGGING_LEVEL_DEBUG*/  0},
        {/*LLM_LOG_VERBOSE*/ 4 ,/*ORT_LOGGING_LEVEL_DEBUG*/  0}
    };

    // Used as a general signal in our LLM module to terminate response
    std::string m_eos = "<|endoftext|>";

    /**
     * Function to initialize the LLM model sequence
     */
     void InitSequence();

    /**
     * Frees the memory holding the LLM Model sequence
     */
     void FreeSequence();

    /**
     * Function to initialize the LLM model configs
     */
     void InitConfigs();

    /**
     * Frees the memory holding the configs
     */
     void FreeConfigs();

    /**
     * Function to initialize a new generator
     */
     void InitGenerator();

    /**
     * Frees the memory holding the generator
     */
    void FreeGenerator();

    /**
     * Function to initialize a new tokenizer
     */
    void InitTokenizer();

    /**
     * Frees the memory holding the tokenizer
     */
     void FreeTokenizer();

    /**
     * Function to load the chosen ONNX model to memory
     */
    void LoadModel();

    /**
     * Frees the memory holding the ONNX model
     */
    void FreeModel();

};

#endif /* LLM_IMPL_HPP */
