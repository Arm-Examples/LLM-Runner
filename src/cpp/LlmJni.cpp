//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include "LlmImpl.hpp"

#include <jni.h>

static std::unique_ptr<LLM> llm{nullptr};

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL Java_com_arm_Llm_createLlmConfig(JNIEnv* env,
                                                         jobject /* this */,
                                                         jstring jModelTag,
                                                         jstring jModelPath,
                                                         jstring jLlmPrefix,
                                                         jint jNumThreads,
                                                         jint jBatchSize)
{
    const char* modelTag  = env->GetStringUTFChars(jModelTag, nullptr);
    const char* modelPath = env->GetStringUTFChars(jModelPath, nullptr);
    const char* llmPrefix = env->GetStringUTFChars(jLlmPrefix, nullptr);

    auto* config = new LlmConfig(std::string(modelTag),
                                 std::string(modelPath),
                                 std::string(llmPrefix),
                                 static_cast<int>(jNumThreads),
                                 static_cast<int>(jBatchSize));

    // Clean up
    env->ReleaseStringUTFChars(jModelTag, modelTag);
    env->ReleaseStringUTFChars(jModelPath, modelPath);
    env->ReleaseStringUTFChars(jLlmPrefix, llmPrefix);

    return reinterpret_cast<jlong>(config); // Return pointer as long
}

JNIEXPORT jlong JNICALL Java_com_arm_Llm_loadModel(JNIEnv* env, jobject, jlong pconfig)
{
    auto config = reinterpret_cast<LlmConfig*>(pconfig);
    llm         = std::make_unique<LLM>();
    llm->LlmInit(*config);
    return 0;
}

JNIEXPORT void JNICALL Java_com_arm_Llm_freeLlm(JNIEnv*, jobject)
{
    llm->FreeLlm();
}

JNIEXPORT void JNICALL Java_com_arm_Llm_encode(JNIEnv* env, jobject, jstring jtext)
{
    const auto text = env->GetStringUTFChars(jtext, 0);
    llm->Encode(text);
    env->ReleaseStringUTFChars(jtext, text);
}

JNIEXPORT jstring JNICALL Java_com_arm_Llm_getNextToken(JNIEnv* env, jobject)
{
    std::string result = llm->NextToken();
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT jfloat JNICALL Java_com_arm_Llm_getEncodeRate(JNIEnv* env, jobject)
{
    float result = llm->GetEncodeTimings();
    return result;
}

JNIEXPORT jfloat JNICALL Java_com_arm_Llm_getDecodeRate(JNIEnv* env, jobject)
{

    float result = llm->GetDecodeTimings();
    return result;
}

JNIEXPORT void JNICALL Java_com_arm_Llm_resetTimings(JNIEnv* env, jobject)
{
    llm->ResetTimings();
}
JNIEXPORT jsize JNICALL Java_com_arm_Llm_getChatProgress(JNIEnv* env, jobject)
{
    return llm->GetChatProgress();
}
JNIEXPORT void JNICALL Java_com_arm_Llm_resetContext(JNIEnv* env, jobject)
{
    llm->ResetContext();
}
JNIEXPORT jstring JNICALL Java_com_arm_Llm_benchModel(
    JNIEnv* env, jobject, jint nPrompts, jint nEvalPrompts, jint nMaxSeq, jint nRep)
{
    std::string result = llm->BenchModel(nPrompts, nEvalPrompts, nMaxSeq, nRep);
    return env->NewStringUTF(result.c_str());
}

#ifdef __cplusplus
}
#endif
