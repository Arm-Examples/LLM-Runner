//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_IMPL_HPP
#define LLM_IMPL_HPP

#include "Llm.hpp"
#include "LlmConfig.hpp"

#include "common.h"
#include "llama.h"

#include <cmath>
#include <string>

/* Forward declaration */
class LLM;

/**
 * @brief LLama Implementation of our LLM API
 */
class LLM::LLMImpl {

public:
    LLMImpl();
    ~LLMImpl();

    /**
     * Method to Initialize a llama_model
     * @param config Configuration class with model's parameter and user defined parameters
     */
    void LlmInit(const LlmConfig& config);

    /**
     * Method to Free all allocations pertaining to llama model
     */
    void FreeLlm();

    /**
     * Function to retrieve the llama encode timings.
     * @return The encoded tokens per second
     */
    float GetEncodeTimings();

    /**
     * Function to retrieve the llama decode timings.
     * @return The decoded tokens per second
     */
    float GetDecodeTimings();

    /**
     * Function to reset the llama timing
     */
    void ResetTimings();

    /**
     * Function to print the system info
     * @return System info as a char pointer
     */
    std::string SystemInfo();

    /**
     * Method to reset Conversation history and preserve Model's character prefix.
     * If model's prefix is not defined all conversation history would be cleared
     */
    void ResetContext();

    /**
     * Method to wrap CompletionInit function with batching and length fitness
     * @param prompt Query to LLM
     */
    void Encode(std::string& prompt);

    /**
     * Method to wrap CompletionLoop function
     * @return the next Token for Encoded Prompt
     */
    std::string NextToken();

    /**
     * @brief The Method return the percentage of chat context filled
     * @return chat capacity filled in cache as percentage number
     */
    size_t GetChatProgress() const;

    /**
     * @brief Benchmarks the performance of the LLM model.
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

private:
    llama_context* m_llmContext{nullptr};
    llama_model* m_llmModel{nullptr};
    llama_batch m_llmBatch{};
    llama_sampler* m_pLlmSampler{nullptr};
    size_t m_batchSz{0};
    int m_nCtx{2048};
    std::string m_cachedTokenChars{""};
    size_t m_contextFilled{0};
    std::string m_llmPrefix{""};
    bool m_llmInitialized{false};
    size_t m_nCur{0};

    /**
     * Function to load the chosen llama model to memory
     * @param pathToModel path to the model location
     * @return llama_model or null-pointer if no model is found
     */
    void LoadModel(const char* pathToModel);

    /**
     * Function to create a new llama_context object in memory
     * @param numThreads number of threads to set in the context
     */
    void NewContext(int numThreads);

    /**
     * Frees the memory holding the llama_model
     */
    void FreeModel();

    /**
     * Free up the memory that is storing the llama_context
     */
    void FreeContext();

    /**
     * Function to initialize the llama backend
     */
    void BackendInit();

    /**
     * Function to free up the memory storing the backend
     */
    void BackendFree();

    /**
     * Function to free up the memory storing the Batch instance
     */
    void FreeBatch();

    /**
     * Function to free Sampler
     */
    void FreeSampler();

    /**
     * Function to clear KV Cache and hence all conversation history
     */
    void KVCacheClear();

    /**
     * Function to removes all tokens that belong to the last sequence(-1) and have positions in
     * [p0, p1)
     * @param p0
     * @param p1
     */
    void KVCacheSeqRm(int32_t p0, int p1);

    /**
     * Function to tokenize the initial prompt
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
     * Function to initialize batch object
     * @param numTokens
     * @param embeddings
     * @param numSequenceMax
     * @return batch object
     */
    llama_batch NewBatch(int numTokens, int embeddings, int numSequenceMax);

    /**
     * Function to Create a new sampler object
     * @return Initialised sampler object
     */

    void NewSampler();

    /**Taken from llama.cpp/examples/llama.android/llama/src/main/cpp/llama-android.cpp and slightly
     * modified
     * @param sub_tokens_list a vector of tokens to encode into llama model
     * @param lastBatch whether the current batch is last set of tokens in given query.
     */
    void CompletionInit(llama_tokens sub_tokens_list, bool lastBatch);

    /**
     * @brief Generates a token completion for the given context and batch.
     *
     * This function processes the current context and batch to generate the next token in the
     *sequence. It utilizes the model's vocabulary and sampling methods to produce a token, which is
     *then converted to a string representation. The function also handles end-of-sequence tokens
     *and ensures UTF-8 validity of the generated token.
     *.
     * @return The generated token as a string. Returns "<|endoftext|>" if the end-of-sequence token
     *is produced or if the current length reaches the maximum length.
     */
    std::string CompletionLoop();
};

#endif /* LLM_IMPL_HPP */
