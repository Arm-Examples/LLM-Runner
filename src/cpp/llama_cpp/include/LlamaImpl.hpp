//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

# pragma once

#include <string>
#include <cmath>
#include "llama.h"
#include "common.h"
#include "LLM.hpp"

#define LOG_INF(...) do { fprintf(stdout, __VA_ARGS__); } while (0)

/**
* @brief LLama Implementation of our LLM API
*
*/
class LlamaImpl : public LLM<LlamaImpl>
{
private:

public:
    LlamaImpl() = default;

    /**
     * Function to load the chosen llama model to memory
     * @tparam P llama_model
     * @param path_to_model path to the model location
     * @return llama_model or null pointer if no model is found
     */
    template<typename P>
    P *LoadModel(const char *path_to_model)
    {
        const llama_model_params model_params = llama_model_default_params();
        auto model = llama_model_load_from_file(path_to_model, model_params);
        if (model == nullptr) {
            fprintf(stderr , "%s: error: unable to load model\n" , __func__);
            return model;
        }

        return model;
    }

    /**
     * Free the memory holding the llama_model
     * @tparam P llama_model
     * @param model the pointer to the llama_model
     */
    template<typename P>
    void FreeModel(P *model)
    {
        llama_model_free(model);
    }

    /**
     * Function to create a new llama_context object in memory
     * @tparam P llama_context
     * @tparam V llama_model
     * @param model LLM model pointer
     * @param numThreads number of threads to set in the context
     * @return LLM context object pointer
     */
    template<typename P, typename V>
    P *NewContext(V *model, const int numThreads)
    {
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = 2048;
        ctx_params.n_threads = numThreads;
        ctx_params.n_threads_batch = numThreads;
        ctx_params.no_perf = false;

        llama_context *context = llama_init_from_model(model, ctx_params);

        return context;
    }

    /**
     * Free up the memory that is storing the llama_context
     * @tparam P llama_context
     * @param llamaContext LLM context pointer
     */
    template<typename P>
    void FreeContext(P *llamaContext)
    {
        llama_free(llamaContext);
    }

    /**
     * Function to initialize the llama backend
     */
    void BackendInit()
    {
        llama_backend_init();
    }

    /**
     * Function to free up the memory storing the backend
     */
    void BackendFree()
    {
        llama_backend_free();
    }

    /**
     * Function to free up the memory storing the Batch instance
     * @tparam P llama_batch
     * @param batch LLM Batch object pointer
     */
    template<typename P>
    void FreeBatch(P &batch)
    {
        llama_batch_free(batch);
    }

    /**
     * Function to retrieve the llama encode timings
     * @tparam P llama_context
     * @param context LLM Context object pointer
     * @return The encoded tokens per second
     */
    template<typename P>
    float GetEncodeTimings(P *context)
    {
        auto resultsTiming = llama_perf_context(context);
        return (1e3 / resultsTiming.t_p_eval_ms * resultsTiming.n_p_eval);
    }

    /**
     * Function to retrieve the llama decode timings
     * @tparam P llama_context
     * @param context LLM Context object pointer
     * @return The decoded tokens per second
     */
    template<typename P>
    float GetDecodeTimings(P *context)
    {
        auto resultsTiming = llama_perf_context(context);
        return (1e3 / resultsTiming.t_eval_ms * resultsTiming.n_eval);
    }
    /**
     * Function to reset the llama timings
     * @tparam P llama_context
     * @param context LLM context object pointer
     */
    template<typename P>
    void ResetTimings(P *context)
    {
        llama_perf_context_reset(context);
    }

    /**
     * Function to print the system info
     * @return System info as a char pointer
     */
    const char *SystemInfo()
    {
        return llama_print_system_info();
    }

    /**
     * Function to clear KV Cache
     * @tparam P llama_context
     * @param context LLM Context object pointer
     */
    template<typename P>
    void KVCacheClear(P *context)
    {
        llama_kv_cache_clear(context);
    }

    /**
     * Function to remove all tokens that belong to the specified sequence and have positions in [p0, p1)
     * @tparam P llama_context
     * @param context LLM Context object
     * @param p0 starting token index (inclusive)
     * @param p1 ending token index (exclusive)
     */
    template<typename P>
    void KVCacheSeqRm(P *context, int p0, int p1)
    {
        llama_kv_cache_seq_rm(context, -1, p0, p1);
    }

    /**
     * Function to tokenize the initial prompt
     * @tparam P llama_model
     * @tparam V llama tokens container
     * @param model pointer to the LLM model
     * @param text prompt text to be tokenized
     * @param textLength length of the prompt text in bytes (or characters)
     * @param tokens pointer to the container/array that will hold the resulting tokens
     * @param maxNumTokens maximum number of tokens
     * @param addSpecial if `true`, includes special tokens (e.g., BOS/EOS) in the output
     * @param parseSpecial if `true`, parses special tokens directly from the prompt text
     * @return length of original prompt
     */
    template<typename P, typename V>
    int GetInitialPromptLength(P *model, const char *text, int32_t textLength, V *tokens,
                               int32_t maxNumTokens, bool addSpecial, bool parseSpecial)
    {
        const llama_vocab * vocab = llama_model_get_vocab(model);
        return llama_tokenize(vocab,
                              text,
                              textLength,
                              tokens,
                              maxNumTokens,
                              addSpecial,
                              parseSpecial);
    }

    /**
     * Function to Create a new batch object
     * @tparam P llama_batch
     * @param embeddings embedding dimension for each token
     * @param numTokens maximum number of tokens in the batch
     * @param numSequenceMax maximum number of sequences or contexts
     * @return newly created batch object
     */
    template<typename P>
    P NewBatch(int numTokens, int embeddings, int numSequenceMax)
    {
        llama_batch batch = llama_batch_init(numTokens, embeddings, numSequenceMax);
        return batch;
    }

    /**
     * Function to Create a new sampler object
     * @param p_llama_sampler
     * @return Initialised sampler object
     */

    llama_sampler *NewSampler(llama_sampler *p_llama_sampler)
    {
        auto sampler_params = llama_sampler_chain_default_params();
        sampler_params.no_perf = false;

        p_llama_sampler = llama_sampler_chain_init(sampler_params);
        llama_sampler_chain_add(p_llama_sampler, llama_sampler_init_greedy());
        return p_llama_sampler;
    }

    /** Encode the prompt text, inspired by llama.cpp Android example
     * Use 0 for startPos else nCur
     * @tparam P llama context
     * @tparam V llama batch
     * @param text input prompt text to be encoded
     * @param context pointer to the LLM context
     * @param batch pointer to the LLM batch
     * @param startPos start position of the text that will be used
     * @return number of tokens if successful otherwise error code
     */
    template<typename P, typename V>
    int CompletionInit(std::string &text, P *context, V *batch, int &startPos)
    {
        //Synchronize llama to remove idle time between function calls
        llama_synchronize(context);

        const auto tokens_list = common_tokenize(context, text, 1);
        common_batch_clear(*batch);
        // evaluate the initial prompt
        for (auto i = startPos; i < tokens_list.size() + startPos; i++)
        {
            common_batch_add(*batch, tokens_list[i - startPos], i, {0}, false);
        }

        // llama_decode will output logits only for the last token of the prompt
        batch->logits[batch->n_tokens - 1] = true;
        if (llama_decode(context, *batch) != 0)
        {
            LOG_INF("llama_decode() failed");
            return 1;
        }
        llama_synchronize(context);
        return batch->n_tokens;
    }

    /**
     * @brief Generates a token completion for the given context and batch
     *
     * This function processes the current context and batch to generate the next token in the sequence.
     * It utilizes the model's vocabulary and sampling methods to produce a token, which is then converted
     * to a string representation. The function also handles end-of-sequence tokens and ensures UTF-8 validity
     * of the generated token
     *
     * @tparam P Type representing the context, typically `llama_context`
     * @tparam V Type representing the batch, typically `llama_batch`
     * @param context pointer to the LLM context object
     * @param batch pointer to the LLM batch object
     * @param nCur reference to the current length of the sequence
     * @param nLen reference to the maximum length of the sequence
     * @return generated token as a string. Returns "<|endoftext|>" if the end-of-sequence token is produced
     *         or if the current length reaches the maximum length
     */
    template<typename P, typename V>
    std::string CompletionLoop(P *context, V *batch, int &nCur, int &nLen)
    {
        const auto model = llama_get_model(context);
        std::string test;

        const llama_vocab * vocab = llama_model_get_vocab(model);
        auto n_vocab = llama_vocab_n_tokens(vocab);

        auto logits = llama_get_logits_ith(context, batch->n_tokens - 1);

        std::vector<llama_token_data> candidates;
        candidates.reserve(n_vocab);
        for (llama_token token_id = 0; token_id < n_vocab; token_id++)
        {
            candidates.emplace_back(llama_token_data{token_id, logits[token_id], 0.0f});
        }

        llama_sampler *sampler_pointer = NewSampler(sampler_pointer);

        const auto new_token_id = llama_sampler_sample(sampler_pointer, context, -1);
        
        if ((llama_vocab_eos(vocab) == new_token_id) || (nCur == nLen))
        {
            return "<|endoftext|>";
        }

        auto new_token_chars = common_token_to_piece(context, new_token_id);
        cached_token_chars += new_token_chars;
        std::string new_token;
        if (is_valid_utf8(cached_token_chars.c_str()))
        {
            new_token = cached_token_chars;
            cached_token_chars.clear();
        } else
        {
            new_token = "";
        }
        common_batch_clear(*batch);
        common_batch_add(*batch, new_token_id, nCur, {0}, true);

        if (llama_decode(context, *batch) != 0)
        {
            LOG_INF("llama_decode() failed");
        }

        //Synchronize llama to remove idle time between function calls
        llama_synchronize(context);

        ++nCur;
        return new_token;
    }

    /**
     * @brief Checks if a given string is valid UTF-8
     *
     * This function validates whether the input C-string adheres to the UTF-8 encoding standard.
     * It iterates through each byte of the string, determining the expected length of UTF-8 sequences
     * based on leading byte patterns, and verifies that subsequent bytes match the UTF-8 format
     *
     * @param string Pointer to a null-terminated C-string to be validated
     * @return true if the string is valid UTF-8 or if the input is a null pointer; false otherwise
     */
    bool is_valid_utf8(const char *string)
    {
        if (!string)
        {
            return true;
        }

        const auto *bytes = reinterpret_cast<const unsigned char *>(string);
        int num;

        while (*bytes != 0x00)
        {
            if ((*bytes & 0x80) == 0x00)
            {
                // U+0000 to U+007F
                num = 1;
            } else if ((*bytes & 0xE0) == 0xC0)
            {
                // U+0080 to U+07FF
                num = 2;
            } else if ((*bytes & 0xF0) == 0xE0)
            {
                // U+0800 to U+FFFF
                num = 3;
            } else if ((*bytes & 0xF8) == 0xF0)
            {
                // U+10000 to U+10FFFF
                num = 4;
            } else
            {
                return false;
            }

            bytes += 1;
            for (int i = 1; i < num; ++i)
            {
                if ((*bytes & 0xC0) != 0x80)
                {
                    return false;
                }
                bytes += 1;
            }
        }
        return true;
    }
};
