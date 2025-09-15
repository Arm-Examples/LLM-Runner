//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include "LlmImpl.hpp"
#include <iostream>
#include <jni.h>

static std::unique_ptr<LLM> llm;

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_com_arm_Llm_llmInit(JNIEnv* env,
                                                jobject /* this */,
                                                jstring modelJsonStr)
{

    if (modelJsonStr == nullptr) {
        std::cerr << "modelJsonStr is null" << std::endl;
    }

    const char* modelCStr = env->GetStringUTFChars(modelJsonStr, nullptr);
    if (modelCStr == nullptr) {
        std::cerr << "GetStringUTFChars returned null" << std::endl;
    }

    std::string jsonStr(modelCStr);
    env->ReleaseStringUTFChars(modelJsonStr, modelCStr);

    try {
        LlmConfig config = LlmConfig(jsonStr);
        llm = std::make_unique<LLM>(config);
        llm->LlmInit(config);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create Llm from config string: " << e.what() << std::endl;
    }
}

JNIEXPORT void JNICALL Java_com_arm_Llm_freeLlm(JNIEnv*, jobject)
{
    llm->FreeLlm();
}

JNIEXPORT void JNICALL Java_com_arm_Llm_encode(JNIEnv *env, jobject thiz, jstring jtext, jstring path_to_image,
                                               jboolean is_first_message) {
    const auto text = env->GetStringUTFChars(jtext, 0);
    const auto imagePath = env->GetStringUTFChars(path_to_image, 0);
    LLM::EncodePayload payload{text, imagePath, static_cast<bool>(is_first_message)};
    llm->Encode(payload);
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

JNIEXPORT jstring JNICALL Java_com_arm_Llm_getFrameworkType(JNIEnv* env, jobject)
{
    std::string frameworkType = llm->GetFrameworkType();
    return env->NewStringUTF(frameworkType.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_arm_Llm_supportsImageInput(JNIEnv *env, jobject thiz) {
    for (const auto &m : llm->SupportedInputModalities()) {
        if (m.find("image") != std::string::npos) {
            return JNI_TRUE;
        }
    }
    return JNI_FALSE;
}

#ifdef __cplusplus
}
#endif

