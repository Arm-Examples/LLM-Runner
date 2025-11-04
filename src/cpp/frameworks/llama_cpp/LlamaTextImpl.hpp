//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "LlmConfig.hpp"
#include "Llm.hpp"

#include "common.h"
#include "llama.h"

#include <string>


/**
 * @brief LLama Implementation of our LLM API
 */
class LLM::LLMImpl : public LlmChat {

public:
    LLMImpl();
    ~LLMImpl();

    /**
     * @brief Method to initialize a llama_model
     * @param config Configuration class with model's parameter and user defined parameters
     * @param sharedLibraryPath path to location of shared libs
     */
    virtual void LlmInit(const LlmConfig& config, std::string sharedLibraryPath = "");

    /**
     * @brief Method to free all allocations pertaining to llama model
     */
    virtual void FreeLlm();

    /**
     * @brief Function to retrieve the llama encode timings.
     * @return The encoded tokens per second
     */
    virtual float GetEncodeTimings();

    /**
     * @brief Function to retrieve the llama decode timings.
     * @return The decoded tokens per second
     */
    virtual float GetDecodeTimings();

    /**
     * @brief Function to reset the llama timing
     */
    virtual void ResetTimings();

    /**
     * @brief Function to print the system info
     * @return System info as a char pointer
     */
    virtual std::string SystemInfo();

    /**
     * @brief Method to reset conversation history and preserve model's character prefix.
     * If model's prefix is not defined all conversation history would be cleared
     */
    virtual void ResetContext();

    /**
     * @brief Encode a multimodal payload (text + optional image).
     * @param payload Input payload containing text and/or image path.
     */
    virtual void Encode(LlmChat::Payload& payload);

    /**
     * @brief Method to wrap CompletionLoop function
     * @return the next token for encoded prompt
     */
    virtual std::string NextToken();

    /**
     * @brief The Method return the percentage of chat context filled
     * @return chat capacity filled in cache as percentage number
     */
    virtual size_t GetChatProgress() const;

    /**
     * Benchmarks the performance of the LLM model.
     *
     * This function evaluates the model's performance by processing a specified number of prompts
     * and generating text sequences. It measures the speed of prompt evaluation and text
     * generation, calculates average speeds and standard deviations over multiple repetitions, and
     * compiles the results into a formatted string.
     *
     * @param prompts Number of prompt tokens to process during benchmarking.
     * @param eval_prompts Number of evaluation prompts for text generation.
     * @param n_max_sq Maximum sequence length for text generation.
     * @param n_rep Number of repetitions for benchmarking to obtain average metrics.
     * @return A formatted string containing the benchmark results, including model description,
     * size, number of parameters, backend information, and performance metrics for prompt
     * evaluation and text generation.
     */
    virtual std::string BenchModel(int& prompts, int& eval_prompts, int& n_max_sq, int& n_rep);

    /**
     * @brief Method to get framework type
     * @return string framework type
     */
    static std::string GetFrameworkType() {return "llama.cpp";}

    /**
     * @brief Method to Cancel generation of response tokens. Can be used to stop response once query commences
     */
    void StopGeneration();

    /**
     * @brief List supported input modalities.
     * @return A vector containing {"text", "vision"}.
     */
    virtual std::vector<std::string> SupportedInputModalities() const{  return {"text"};}

    /**
     * Applies the automatic chat template to the given prompt.
     * @param payload The input prompt to apply the template to.
     * @return The prompt with the automatic chat template applied.
     */
    bool ApplyAutoChatTemplate(LlmChat::Payload& payload) override;

protected:
    llama_context* m_llmContext{nullptr};     /**< Pointer to the llama model context. */
    llama_model* m_llmModel{nullptr};         /**< Pointer to the loaded llama model. */
    llama_batch m_llmBatch{};                 /**< Batch object for processing tokens. */
    llama_sampler* m_pLlmSampler{nullptr};    /**< Sampler used for token generation. */
    size_t m_batchSz{0};                      /**< Current batch size. */
    int m_nCtx{0};                            /**< Maximum context window size. */
    std::string m_cachedTokenChars{""};       /**< Cached decoded token characters. */
    size_t m_contextFilled{0};                /**< Proportion of the context window currently filled (as % of total tokens). */
    std::string m_llmPrefix{""};              /**< Prefix prepended to prompts. */
    bool m_llmInitialized{false};             /**< Indicates whether the LLM is initialized. */
    size_t m_nCur{0};                         /**< Current token index in the context. */
    std::string m_eos = "<|endoftext|>";      /**< Used as a general signal in our LLM module to terminate response. */
    LlmConfig m_config;                       /**< Configuration for model. */
    /**
     * @brief Function to load the chosen llama model to memory
     */
    virtual void LoadModel();

    /**
     * @brief Function to create a new llama_context object in memory
     */
    virtual void NewContext();

    /**
     * @brief Frees the memory holding the llama_model
     */
    void FreeModel();

    /**
     * @brief Free up the memory that is storing the llama_context
     */
    void FreeContext();

    /**
     * @brief Function to initialize the llama backend
     */
    void BackendInit();

    /**
     * @brief Function to free up the memory storing the backend
     */
    void BackendFree();

    /**
     * @brief Function to free up the memory storing the Batch instance
     */
    void FreeBatch();

    /**
     * @brief Function to free Sampler
     */
    void FreeSampler();

    /**
     * @brief Function to clear KV Cache and hence all conversation history
     */
    void KVCacheClear();

    /**
     * @brief Function to removes all tokens that belong to the last sequence(-1) and have positions in
     * [p0, p1)
     * @param p0
     * @param p1
     */
    void KVCacheSeqRm(int32_t p0, int p1);

    /**
     * @brief Function to tokenize the initial prompt
     * @param text
     * @param textLength
     * @param addSpecial
     * @param parseSpecial
     * @return length of original prompt
     */

    int32_t GetInitialPromptLength(const char* text,
                                   int32_t textLength,
                                   bool addSpecial,
                                   bool parseSpecial);

    /**
     * @brief Function to initialize batch object
     * @param numTokens
     * @param embeddings
     * @param numSequenceMax
     * @return batch object
     */
    llama_batch NewBatch(int numTokens, int embeddings, int numSequenceMax);

    /**
     * @brief Function to create a new sampler object
     */
    virtual void NewSampler();

    /**
     * @brief Taken from llama.cpp/examples/llama.android/llama/src/main/cpp/llama-android.cpp and
     * slightly modified
     * @param sub_tokens_list a vector of tokens to encode into llama model
     * @param lastBatch whether the current batch is last set of tokens in given query.
     */
    void CompletionInit(llama_tokens sub_tokens_list, bool lastBatch);

    /**
     * @brief Generates a token completion for the given context and batch.
     *
     * This function processes the current context and batch to generate the next token in the
     * sequence. It utilizes the model's vocabulary and sampling methods to produce a token, which is
     * then converted to a string representation. The function also handles end-of-sequence tokens
     * and ensures UTF-8 validity of the generated token.
     * @return The generated token as a string. Returns "<|endoftext|>" if the end-of-sequence token
     * is produced or if the current length reaches the maximum length.
     */
    std::string CompletionLoop();

    /**
     * We are using our Logging functions and overriding default ggml logging callback.
     * @param level The ggml_log_level tag used by llama.cpp to identify whther it is ERROR,WARN or INFO
     * @param text The full logging text including formatting information
     * @param user_data The context for user to store extra details on logging.
     */
    static void llama_llm_log_callback(enum ggml_log_level level, const char * text, void * user_data);

};
