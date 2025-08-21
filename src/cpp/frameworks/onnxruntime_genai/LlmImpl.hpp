//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_IMPL_HPP
#define LLM_IMPL_HPP

#include "Llm.hpp"
#include "LlmConfig.hpp"

#include "ort_genai.h"

/* Forward declaration */
class LLM;

/**
 * @brief ONNX Implementation of our LLM API
 */
class LLM::LLMImpl {

public:
    LLMImpl();
    ~LLMImpl();

    /**
     * Method to initialize a ONNX model
     * @param config Configuration class with model's parameter and user defined parameters
     */
    void LlmInit(const LlmConfig& config);

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
     * Method to prompt encoding
     * @param prompt Query to LLM
     */
    void Encode(EncodePayload& prompt);

    /**
     * Builds a query string for the LLM based on the given prompt.
     * @param prompt The prompt to structure into a query.
     * @return The constructed query string.
     */
    std::string QueryBuilder(std::string&prompt);

    /**
     * Method to produce next token
     * @return the next token for encoded prompt
     */
    std::string NextToken();

    /**
     * The method return the percentage of chat context filled
     * @return chat capacity filled in cache as percentage number
     */
    size_t GetChatProgress() const;

    /**
     * Benchmarks the performance of the LLM model.
     *
     * This function evaluates the model's performance by processing a specified number of prompts
     * and generating text sequences. It measures the speed of prompt evaluation and text
     * generation, calculates average speeds and standard deviations over multiple repetitions, and
     * compiles the results into a formatted string.
     *
     * @param prompts Number of prompts to process during benchmarking.
     * @param eval_prompts Number of evaluation prompts for text generation.
     * @param n_max_sq Maximum sequence length for text generation.
     * @param n_rep Number of repetitions for benchmarking to obtain average metrics.
     * @return A formatted string containing the benchmark results, including model description,
     * size, number of parameters, backend information, and performance metrics for prompt
     * evaluation and text generation.
     */
    std::string BenchModel(int& prompts, int& eval_prompts, int& n_max_sq, int& n_rep);

    /**
     * Method to get framework type
     * @return string framework type
     */
    std::string GetFrameworkType();

    /**
     * @brief Build and return a query string from the given prompt and configuration.
     * @param prompt Input payload containing text, optional image, and conversation metadata.
     * @return The constructed query string to be passed to the model backend.
     */
    virtual std::string QueryBuilder(EncodePayload& prompt);

    /**
     * @brief List supported input modalities.
     * @return A vector containing {"text", "vision"}.
     */
    std::vector<std::string> SupportedInputModalities() const{  return {"text"};}

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration();

private:
    // Framework type
    std::string m_frameworkType{"onnxruntime-genai"};
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
    int m_nCtx{2048};
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

    // Flag indicating whether this is starting of the conversation (used to decide if the system prompt should be encoded)
    bool m_isConversationStart{true};
    // Flag indicating whether a custom chat template should be used
    bool m_isDefaultTemplate{false};
    // System prompt to be encoded with first query
    std::string m_systemPrompt{""};
    // Default template for system message
    std::string m_systemTemplate{""};
    // Default template for user message
    std::string m_userTemplate{""};
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

    /**
     * Applies a default chat template to the given prompt.
     * @param prompt The input prompt to apply the template to.
     * @return The prompt with the default chat template applied.
     */
    std::string ApplyDefaultChatTemplate(const std::string& prompt);

    /**
     * Applies the automatic chat template to the given prompt.
     * @param prompt The input prompt to apply the template to.
     * @return The prompt with the automatic chat template applied.
     */
    std::string ApplyAutoChatTemplate(const std::string& prompt);
};

#endif /* LLM_IMPL_HPP */
