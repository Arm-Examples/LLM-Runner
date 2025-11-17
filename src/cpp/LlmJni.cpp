//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include "LlmImpl.hpp"
#include "Logger.hpp"
#include <iostream>
#include <jni.h>


static std::unique_ptr<LLM> llm;

/**
* @brief inline method to throw error in java
* @param env JNI environment variable passed from JVM layer
* @param message error message to be propagated to java/kotlin layer
*/
inline void ThrowJavaException(JNIEnv* env, const char* message) {
    jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
    if (exceptionClass) {
        env->ThrowNew(exceptionClass, message);
    }
}
/**
* @brief  Lambda function to realize RAII utf-strings
* @param env JNI environment variable passed from JVM layer
* @param jStr java string variable to be converted to c-string
* @return reference to converted c-string
*/
auto GetUtfChars = [](JNIEnv* env, jstring jStr) {
    using Deleter = std::function<void(const char*)>;

    const char* chars = env->GetStringUTFChars(jStr, nullptr);
    Deleter deleter = [env, jStr](const char* p) {
        env->ReleaseStringUTFChars(jStr, p);
    };

    return std::unique_ptr<const char, Deleter>(chars, deleter);
};


#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT void JNICALL Java_com_arm_Llm_llmInit(JNIEnv* env,
                         jobject /* this */,
                         jstring modelJsonStr,
                         jstring sharedLibraryPath) {
    try {
        if (modelJsonStr == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, error in config json string ");
            return;
        }
        auto modelCStr = GetUtfChars(env,modelJsonStr);
        if (modelCStr.get() == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, jstring to utf conversion failed for config json");
            return;
        }

        if (sharedLibraryPath == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, jstring shared-library-path is null ");
            return;
        }
        auto sharedLibraryPathNative = GetUtfChars(env,sharedLibraryPath);
        if (sharedLibraryPathNative.get() == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, unable to parse shared-library-path into string ");
            return;
        }

        try {
            LlmConfig config(modelCStr.get());
            llm = std::make_unique<LLM>();
            llm->LlmInit(config, sharedLibraryPathNative.get());
        } catch (const std::exception& e) {
            std::string msg = std::string("Failed to create Llm from config : ") + e.what();
            ThrowJavaException(env, msg.c_str());
            return;
        }

    } catch (const std::exception& e) {
        std:: string msg = std::string("Failed to create Llm instance due to error: ") + e.what();
        //  prevents C++ exceptions escaping JNI
        ThrowJavaException(env, msg.c_str());
        return;
    }
}

JNIEXPORT void JNICALL Java_com_arm_Llm_freeLlm(JNIEnv*, jobject)
{
    llm->FreeLlm();
}

JNIEXPORT void JNICALL Java_com_arm_Llm_encode(JNIEnv *env, jobject thiz, jstring jtext, jstring path_to_image,
                                               jboolean is_first_message) {
    try {
        auto textChars = GetUtfChars(env,jtext);
        auto imageChars = GetUtfChars(env,path_to_image);
        std::string text(textChars.get());
        std::string imagePath(imageChars.get());
        LlmChat::Payload payload{text, imagePath, static_cast<bool>(is_first_message)};
        llm->Encode(payload);
    }
    catch (const std::exception& e) {
        ThrowJavaException(env, ("Failed to encode query: " + std::string(e.what())).c_str());
        return;
    }
}

JNIEXPORT jstring JNICALL Java_com_arm_Llm_getNextToken(JNIEnv* env, jobject)
{
    try {
        std::string result = llm->NextToken();
        return env->NewStringUTF(result.c_str());
    }
    catch (const std::exception& e) {
        std::string msg = std::string("Failed to get next token: ") + e.what();
        ThrowJavaException(env,msg.c_str() );
        return nullptr;
    }
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
    try {
        llm->ResetContext();
    }
    catch (const std::exception& e) {
        std::string msg = std::string("Failed to reset context: ") + e.what();
        ThrowJavaException(env,msg.c_str() );
        return;
    }
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
