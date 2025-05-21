//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include <jni.h>
#include <iostream>
#include "LlamaImpl.hpp"
#include "LLM.hpp"
#include <cstring>
#include <cmath>
#include <thread>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load llama model from path
 * @param env JNI environment
 * @param path_to_model path to llama model
 * @return pointer to llama model
 */
JNIEXPORT jlong JNICALL Java_com_arm_llm_Llama_loadModel(JNIEnv *env, jobject, jstring path_to_model)
{

    const char *path = env->GetStringUTFChars(path_to_model, nullptr);
    if (path == nullptr || strlen(path) == 0)
    {
        env->ReleaseStringUTFChars(path_to_model, path);
        return 0;
    }

    auto *llm = new LLM<LlamaImpl>();
    auto *model = llm->LoadModel<llama_model>(path);
    env->ReleaseStringUTFChars(path_to_model, path);
    return reinterpret_cast<jlong>(model);
}

/**
 * Perform KV cache clear
 * @param contextPtr pointer to the model context
 */
JNIEXPORT void JNICALL
Java_com_arm_llm_Llama_kvCacheClear(JNIEnv, jobject, jlong contextPtr)
{
    llama_kv_self_clear(reinterpret_cast<llama_context *>(contextPtr));
}

/**
 * Remove all tokens that belong to the specified sequence
 * @param contextPtr pointer to the model context
 * @param start_pos starting position of sequence
 * @param last_pos last position of sequence
 */
JNIEXPORT void JNICALL
Java_com_arm_llm_Llama_kvCacheSeqRm(JNIEnv, jobject, jlong contextPtr, jint start_pos, jint last_pos)
{
    llama_kv_self_seq_rm(reinterpret_cast<llama_context *>(contextPtr), -1, start_pos, last_pos);
}

/**
 * Computes the token length of the given prompt text using
 * @param env JNI environment
 * @param model_ptr pointer to llama model
 * @param text_length length of the prompt text
 * @param jtext string containing the prompt text to be tokenized
 * @param add_special  if true, includes special tokens
 * @return the number of tokens generated from the text
 */
JNIEXPORT jint JNICALL
Java_com_arm_llm_Llama_getInitialPromptLength(JNIEnv *env, jobject, jlong model_ptr, jint text_length, jstring jtext,
                                          jboolean add_special)
{
    auto *model = reinterpret_cast<llama_model *>(model_ptr);

    const auto text = env->GetStringUTFChars(jtext, nullptr);
    bool parse_special = false;
    int max_num_tokens = 1024;
    auto tokens = static_cast<llama_token *>(malloc(sizeof(llama_token) * max_num_tokens));
    const llama_vocab * vocab = llama_model_get_vocab(model);
    return llama_tokenize(vocab,text,text_length,tokens,max_num_tokens,add_special,parse_special);

}

/**
 * Frees the model, associated context, and llama backend resources
 * @param model pointer to llama model
 * @param contextPtr pointer to the model context
 */
JNIEXPORT void JNICALL
Java_com_arm_llm_Llama_freeModel(JNIEnv *, jobject, jlong model, jlong contextPtr)
{
    llama_free(reinterpret_cast<llama_context *>(contextPtr));
    llama_model_free(reinterpret_cast<llama_model *>(model));
    llama_backend_free();
}

/**
 * Initialize llama backend
 */
JNIEXPORT void JNICALL Java_com_arm_llm_Llama_backendInit(JNIEnv, jobject)
{
    auto *llm = new LLM<LlamaImpl>();
    llm->BackendInit();
}

/**
 * Creates and initializes a new sampler chain with default parameters
 * @return pointer to the newly created sampler chain
 */
JNIEXPORT jlong JNICALL
Java_com_arm_llm_Llama_newSampler(JNIEnv *, jobject)
{
    auto sampler_params = llama_sampler_chain_default_params();
    llama_sampler *smpl = llama_sampler_chain_init(sampler_params);
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.0));
    return reinterpret_cast<jlong>(smpl);
}

/**
 * Frees a sampler previously created new sampler
 */
JNIEXPORT void JNICALL
Java_com_arm_llm_Llama_freeSampler(JNIEnv *, jobject, jlong sampler_pointer)
{
    llama_sampler_free(reinterpret_cast<llama_sampler *>(sampler_pointer));
}

/**
 * Creates a new llama batch
 * @param n_tokens total number of tokens
 * @param embd embedding dimension
 * @param n_seq_max The maximum number of sequences
 * @return pointer to the newly allocated llama batch
 */
JNIEXPORT jlong JNICALL Java_com_arm_llm_Llama_newBatch(JNIEnv *, jobject, jint n_tokens, jint embd, jint n_seq_max)
{

    auto *batch = new llama_batch{
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };

    if (embd)
    {
        batch->embd = static_cast<float *>(malloc(sizeof(float) * n_tokens * embd));
    } else
    {
        batch->token = static_cast<llama_token *>(malloc(sizeof(llama_token) * n_tokens));
    }

    batch->pos = static_cast<llama_pos *>(malloc(sizeof(llama_pos) * n_tokens));
    batch->n_seq_id = static_cast<int32_t *>(malloc(sizeof(int32_t) * n_tokens));
    batch->seq_id = static_cast<llama_seq_id **>(malloc(sizeof(llama_seq_id *) * n_tokens));
    for (int i = 0; i < n_tokens; ++i)
    {
        batch->seq_id[i] = static_cast<llama_seq_id *>(malloc(sizeof(llama_seq_id) * n_seq_max));
    }
    batch->logits = static_cast<int8_t *>(malloc(sizeof(int8_t) * n_tokens));

    return reinterpret_cast<jlong>(batch);
}

/**
 * Create a new context
 * @param env JNI environment
 * @param model_ptr llama model pointer
 * @param numThreads number of threads
 * @return pointer to the new model context
 */
JNIEXPORT jlong JNICALL Java_com_arm_llm_Llama_newContext(JNIEnv *env, jobject, jlong model_ptr, jint numThreads)
{

    auto model = reinterpret_cast<llama_model *>(model_ptr);
    if (!model)
    {
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Model cannot be null");
        return 0;
    }
    auto *llm = new LLM<LlamaImpl>();
    auto *context = llm->NewContext<llama_context, llama_model>(model, numThreads);

    if (!context)
    {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),
                      "llama_new_context_with_model() returned null)");
        return 0;
    }

    return reinterpret_cast<jlong>(context);
}

/**
 * Function used for encoding the prompt text
 * @param env JNI environment
 * @param jtext text containing the prompt to encode
 * @param contextPtr pointer to model context
 * @param batch_pointer pointer to current batch
 * @param start_pos starting position in the text to be encoded
 * @return number of tokens
 */
JNIEXPORT jint JNICALL
Java_com_arm_llm_Llama_completionInit(JNIEnv *env, jobject, jstring jtext, jlong contextPtr, jlong batch_pointer, \
jint start_pos)
{
    const auto text = env->GetStringUTFChars(jtext, nullptr);
    const auto context = reinterpret_cast<llama_context *>(contextPtr);
    const auto batch = reinterpret_cast<llama_batch *>(batch_pointer);
    auto *llm = new LLM<LlamaImpl>();
    int n_tokens = llm->CompletionInit(text, context, batch, start_pos);
    env->ReleaseStringUTFChars(jtext, text);

    return n_tokens;
}

/**
 * This function processes the current context and batch to generate the next token in the sequence.
 * It utilizes the model's vocabulary and sampling methods to produce a token, which is then converted
 * to a string representation. The function also handles end-of-sequence tokens and ensures UTF-8 validity
 * of the generated token
 *
 * @param env JNI environment
 * @param contextPtr pointer to model context
 * @param batchPtr pointer to current batch
 * @param nCur current sequence length
 * @param nLen max sequence length
 * @return the next token
 */
JNIEXPORT jstring JNICALL
Java_com_arm_llm_Llama_completionLoop(JNIEnv *env, jobject, jlong contextPtr, jlong batchPtr, jint nCur, jint nLen)
{
    auto *context = reinterpret_cast<llama_context *>(contextPtr);
    auto *batch = reinterpret_cast<llama_batch *>(batchPtr);
    auto *llm = new LLM<LlamaImpl>();
    std::string result = llm->CompletionLoop(context, batch, nCur, nLen);
    return env->NewStringUTF(result.c_str());
}

/**
 * Get llama encode timings
 * @param contextPtr pointer to the model context
 * @return encode timings
 */
JNIEXPORT jfloat JNICALL
Java_com_arm_llm_Llama_getEncodeTimings(JNIEnv, jobject, jlong contextPtr)
{
    auto *context = reinterpret_cast<llama_context *>(contextPtr);
    auto *llm = new LLM<LlamaImpl>();
    float result = llm->GetEncodeTimings(context);
    return result;
}

/**
 * Get llama decode timings
 * @param contextPtr pointer to the model context
 * @return decode timings
 */
JNIEXPORT jfloat JNICALL
Java_com_arm_llm_Llama_getDecodeTimings(JNIEnv, jobject, jlong contextPtr)
{

    auto resultsTiming = llama_perf_context(reinterpret_cast<llama_context *>(contextPtr));
    return static_cast<float>(1e3 / resultsTiming.t_eval_ms * resultsTiming.n_eval);
}

/**
 * Reset timings recorded previously
 * @param contextPtr pointer to the model context
 */
JNIEXPORT void JNICALL
Java_com_arm_llm_Llama_resetTimings(JNIEnv, jobject, jlong contextPtr)
{
    llama_perf_context_reset(reinterpret_cast<llama_context *>(contextPtr));
}

#ifdef __cplusplus
}
#endif
