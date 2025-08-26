//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include <jni.h>

#include "LlmConfig.hpp"
#include "LlmImpl.hpp"
#include "Logger.hpp"
#include "LlmBridge.hpp"
#include "LlmCache.hpp"


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

const char* ILLEGAL_STATE_EXCEPTION_JAVA_CLASS_NAME = "java/lang/IllegalStateException";
const char* ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE = "LLMHandle invalid, no LLM associated with llmHandle";

#ifdef __cplusplus
extern "C" {
#endif
JavaVM *g_vm = nullptr;
jclass g_NativeBridgeClass = nullptr;          // Global ref to Kotlin NativeBridge
jmethodID g_onNativeComplete = nullptr;        // Cached method ID


/*
 * We attempt to lookup g_nativeBridgeClassName to see if it is loaded in the JVM.
 * If loaded we use callback methods on that class.
 * If the class is not loaded by the JVM we do not attempt to trigger callbacks, this prevents the
 * library having any dependency on RTVA.
 */
const char *g_nativeBridgeClassName = "com/arm/voiceassistant/utils/NativeBridge";

// ------------------------------------------------------------
// Logging helpers
// ------------------------------------------------------------

#define LOGD(fmt, ...)                                              \
        do {                                                                \
            fprintf(stdout, "[%s:%d] " fmt "\n",                            \
                    __FILE__, __LINE__, ##__VA_ARGS__);                     \
            fflush(stdout);                                                 \
        } while (0)


#define LOGE(fmt, ...)                                              \
        do {                                                                \
            fprintf(stdout, "[%s:%d] " fmt "\n",                            \
                    __FILE__, __LINE__, ##__VA_ARGS__);                     \
            fflush(stdout);                                                 \
        } while (0)


/**
 * @brief JNI entry point to initialize an LLM instance and return a handle.
 * @param env JNI environment variable passed from JVM layer
 * @param jsonConfig java string containing the LLM config JSON
 * @param sharedLibraryPath java string containing the path to the framework shared library
 * @return native handle (jlong) to the cached LLM instance, or 0 on failure & Java exception thrown 
 */
JNIEXPORT jlong JNICALL Java_com_arm_Llm_llmInitJNI(JNIEnv* env,
                        jobject /* this */,
                        jstring jsonConfig,
                        jstring sharedLibraryPath) {
    try {
        if (jsonConfig == nullptr) {
                ThrowJavaException(env, "Failed to initialize LLM module, error in config json string ");
                return 0;
        }
        auto modelCStr = GetUtfChars(env, jsonConfig);
        if (modelCStr.get() == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, jstring to utf conversion failed for config json");
            return 0;
        }
        if (sharedLibraryPath == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, jstring shared-library-path is null");
            return 0;
        }
        auto sharedLibraryPathNative = GetUtfChars(env,sharedLibraryPath);
        if (sharedLibraryPathNative.get() == nullptr) {
            ThrowJavaException(env, "Failed to initialize LLM module, unable to parse shared-library-path into string");
            return 0;
        }

        try {
            LlmConfig config(modelCStr.get());
            auto llm = std::make_unique<LLM>();
            llm->LlmInit(config, sharedLibraryPathNative.get());
            return LLMCache::Instance().Add(std::move(llm));
        } catch (const std::exception& e) {
            std::string msg = std::string("Failed to create Llm from config : ") + e.what();
            ThrowJavaException(env, msg.c_str());
            return 0;
        }
    } catch (const std::exception& e) {
        std:: string msg = std::string("Failed to create Llm Instance due to error: ") + e.what();
        //  prevents C++ exceptions escaping JNI
        ThrowJavaException(env, msg.c_str());
    }
    return 0;
}

/**
 * @brief JNI entry point to free an LLM instance referenced by a handle.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 */
JNIEXPORT void JNICALL Java_com_arm_Llm_freeLlmJNI(JNIEnv* env, jobject, jlong llmHandle)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return;
    }

    llm->FreeLlm();
    LLMCache::Instance().Remove(llmHandle);
}

/**
 * @brief JNI entry point to encode a chat payload (text + optional image path).
 * @param env JNI environment variable passed from JVM layer
 * @param jtext java string containing the text prompt
 * @param path_to_image java string containing the path to an input image (may be empty)
 * @param is_first_message boolean indicating whether this is the first message in a chat session
 * @param llmHandle native handle returned by llmInitJNI
 */
JNIEXPORT void JNICALL Java_com_arm_Llm_encodeJNI(JNIEnv *env, jobject thiz, jstring jtext, jstring path_to_image,
                                               jboolean is_first_message, jlong llmHandle) {

    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return;
    }
    
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
    }
}

/**
 * @brief JNI entry point to retrieve the next decoded token from the model.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 * @return java string containing the next token (empty string on invalid handle)
 */
JNIEXPORT jstring JNICALL Java_com_arm_Llm_getNextTokenJNI(JNIEnv* env, jobject, jlong llmHandle)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return env->NewStringUTF("");
    }

    try {
       std::string result = llm->NextToken();
       return env->NewStringUTF(result.c_str());
    } catch (const std::exception& e) {
        std::string msg = std::string("Failed to get next token: ") + e.what();
        ThrowJavaException(env,msg.c_str() );
        return nullptr;
    }
}

/**
 * @brief JNI entry point to retrieve the next decoded token with cancellation support.
 * @param env JNI environment variable passed from JVM layer
 * @param operationId operation identifier used for cancellation
 * @param llmHandle native handle returned by llmInitJNI
 * @return java string containing the next token (empty string on invalid handle)
 */
JNIEXPORT jstring JNICALL Java_com_arm_Llm_getNextTokenCancellableJNI(JNIEnv* env, jobject, jlong operationId, jlong llmHandle)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return env->NewStringUTF("");
    }

    std::string result = llm->CancellableNextToken(operationId);
    return env->NewStringUTF(result.c_str());
}

/**
 * @brief JNI entry point to return the model encode rate/timings.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 * @return encode timing metric as float (0.0 on invalid handle)
 */
JNIEXPORT jfloat JNICALL Java_com_arm_Llm_getEncodeRateJNI(JNIEnv* env, jobject, jlong llmHandle)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return 0.0;
    }

    float result = llm->GetEncodeTimings();
    return result;
}

/**
 * @brief JNI entry point to return the model decode rate/timings.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 * @return decode timing metric as float (0.0 on invalid handle)
 */
JNIEXPORT jfloat JNICALL Java_com_arm_Llm_getDecodeRateJNI(JNIEnv* env, jobject, jlong llmHandle)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return 0.0;
    }

    return llm->GetDecodeTimings();
}

/**
 * @brief JNI entry point to reset encode/decode timing counters for an LLM instance.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 */
JNIEXPORT void JNICALL Java_com_arm_Llm_resetTimingsJNI(JNIEnv* env,  jobject, jlong llmHandle)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);

        return;
    }

    llm->ResetTimings();
}

/**
 * @brief JNI entry point to query current chat progress from an LLM instance.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 * @return progress counter as jsize (0 on invalid handle)
 */
JNIEXPORT jsize JNICALL Java_com_arm_Llm_getChatProgressJNI(JNIEnv* env, jobject, jlong llmHandle)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);

        return 0;
    }

    return llm->GetChatProgress();
}

/**
 * @brief JNI entry point to reset the conversation/context state in an LLM instance.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 */
JNIEXPORT void JNICALL Java_com_arm_Llm_resetContextJNI(JNIEnv* env, jobject, jlong llmHandle)
{
    try {
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return;
    }

    llm->ResetContext();
}
    catch (const std::exception& e) {
        std::string msg = std::string("Failed to reset context: ") + e.what();
        ThrowJavaException(env,msg.c_str() );
        return;
    }
}

/**
 * @brief JNI entry point to run a benchmark on the current model instance.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 * @param nPrompts number of prompts to use for the benchmark
 * @param nEvalPrompts number of evaluation prompts
 * @param nMaxSeq maximum sequence length
 * @param nRep number of repetitions
 * @return java string containing benchmark results (empty string on invalid handle)
 */
JNIEXPORT jstring JNICALL Java_com_arm_Llm_benchModelJNI(
    JNIEnv* env, jobject, jlong llmHandle, jint nPrompts, jint nEvalPrompts, jint nMaxSeq, jint nRep)
{
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return env->NewStringUTF("");
    }


    std::string result = llm->BenchModel(nPrompts, nEvalPrompts, nMaxSeq, nRep);
    return env->NewStringUTF(result.c_str());
}

/**
 * @brief This function returns the LLM Type 
 *
 * We can't lookup the llm from the handle in this function because the llm Instance
 * may not have been created yet.
 *
 * To work around this problem we create an empty LLM Instance that is automatically 
 * deleted by C++ when this function returns. Because this llm object is deleted 
 * calling this function can't be used in lieu of calling initLlm.
 * 
*/
JNIEXPORT jstring JNICALL Java_com_arm_Llm_getFrameworkTypeJNI(JNIEnv* env, jobject, jlong)
{

    auto llm = std::make_unique<LLM>();

    std::string frameworkType = llm->GetFrameworkType();
    return env->NewStringUTF(frameworkType.c_str());
}

/**
 * @brief JNI entry point to determine whether the current LLM supports image input.
 * @param env JNI environment variable passed from JVM layer
 * @param llmHandle native handle returned by llmInitJNI
 * @return JNI_TRUE if "image" is present in supported input modalities, otherwise JNI_FALSE
 */
JNIEXPORT jboolean JNICALL
Java_com_arm_Llm_supportsImageInputJNI(JNIEnv *env, jobject thiz, jlong llmHandle) {
    auto* llm = LLMCache::Instance().Lookup(llmHandle);
    if (!llm) {
        ThrowJavaException(env,ILLEGAL_STATE_EXCEPTION_LOG_MESSAGE);
        return JNI_FALSE;
    }
    
    for (const auto &m : llm->SupportedInputModalities()) {
        if (m.find("image") != std::string::npos) {
            return JNI_TRUE;
        }
    }
    return JNI_FALSE;
}

/**
 * @brief JNI exported method to cancel an in-flight operation identified by operationId.
 * @param env JNI environment variable passed from JVM layer
 * @param operationId operation identifier to cancel
 */
JNIEXPORT void JNICALL
Java_com_arm_voiceassistant_utils_LlmBridge_nativeCancel(
    JNIEnv *env,
    jobject /*clazz*/,
    jlong operationId) {

    auto state = findWork(operationId);
    if (!state) {
        return;
    }

    state->cancelled.store(true, std::memory_order_release);
}


// Inline JNI–version wrapper with customizable args pointer
#if defined(__ANDROID__)
// Android NDK signature: jint AttachCurrentThread(JNIEnv**, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( (penv), (args) ) )
#else
// OpenJDK 11+ signature: jint AttachCurrentThread(void** penv, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( \
         reinterpret_cast<void**>((penv)), (args) \
     ) )
#endif


// Inline JNI–version wrapper with customizable args pointer
#if defined(__ANDROID__)
// Android NDK signature: jint AttachCurrentThread(JNIEnv**, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( (penv), (args) ) )
#else
// OpenJDK 11+ signature: jint AttachCurrentThread(void** penv, void* args)
#  define JNI_ATTACH_CURRENT_THREAD(vm, penv, args) \
     ( (vm)->AttachCurrentThread( \
         reinterpret_cast<void**>((penv)), (args) \
     ) )
#endif

/**
 * @brief JNI_OnLoad callback invoked when the native library is loaded by the JVM.
 *
 * Caches the JavaVM pointer, attempts to resolve the NativeBridge class, and caches
 * the onNativeComplete static method ID for later callbacks.
 *
 * @param vm JavaVM pointer provided by JVM
 * @return JNI version supported on success, JNI_ERR on failure
 */
jint JNI_OnLoad(JavaVM *vm, void *) {
    g_vm = vm;
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    // NOTE: Update this string to match your Kotlin package/class.

    // See comments on g_nativeBridgeClassName for details.
    jclass local = env->FindClass(g_nativeBridgeClassName);
    if (!local) {
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        return JNI_VERSION_1_6;
    }
    g_NativeBridgeClass = reinterpret_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    if (!g_NativeBridgeClass) {
        LOGE("JNI_OnLoad: NewGlobalRef failed");
        return JNI_ERR;
    }
    // Signature: (JILjava/lang/String;)V   => long, int, String -> void
    g_onNativeComplete = env->GetStaticMethodID(
            g_NativeBridgeClass,
            "onNativeComplete",
            "(JILjava/lang/String;)V");
    if (!g_onNativeComplete) {
        LOGE("JNI_OnLoad: cannot find onNativeComplete");
        return JNI_ERR;
    }
    LOGD("JNI_OnLoad success");
    return JNI_VERSION_1_6;
}


/**
 * @brief JNI_OnUnload callback invoked when the native library is unloaded by the JVM.
 *
 * Releases cached global references and clears cached method IDs / VM pointer.
 *
 * @param vm JavaVM pointer provided by JVM
 */
void JNI_OnUnload(JavaVM *vm, void *) {
    if (g_NativeBridgeClass == nullptr) {
        return;
    }

    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {
        env->DeleteGlobalRef(g_NativeBridgeClass);
    }
    g_onNativeComplete = nullptr;
    g_vm = nullptr;
}


/**
 * @struct EnvScope
 * 
 * @brief Stuct used to track JNI state (attached/detached)
 */
struct EnvScope {
    /**
     * @brief JNI environment pointer for the current thread.
     *
     * After successful construction, this should point to a valid JNIEnv
     * that can be used to call into the JVM.
     */
    JNIEnv* env;

    /**
     * @brief Indicates whether this scope attached the current thread.
     *
     * If true, the destructor will detach the current thread from the JVM.
     * If false, the thread was already attached before this EnvScope was
     * created, and the destructor will not attempt to detach it.
     */
    bool didAttach;

    /**
     * @brief Constructs an EnvScope for the current thread.
     *
     * Ensures that the current native thread is attached to the JVM and
     * initializes the env member. Sets didAttach to true if it had to
     * attach the thread, or false if the thread was already attached.
     */
    EnvScope();

    /**
     * @brief Destroys the EnvScope and optionally detaches the thread.
     *
     * If didAttach is true, the destructor detaches the current thread
     * from the JVM. If didAttach is false, no detachment is performed.
     */
    ~EnvScope();
};

/**
 * @brief Constructs an EnvScope for the current thread.
 *
 * Ensures that the current native thread is attached to the JVM and initializes the
 * EnvScope::env member. Sets EnvScope::didAttach to true if it attached the thread,
 * or false if the thread was already attached.
 */
EnvScope::EnvScope() : env(nullptr), didAttach(false) {
    if (g_vm == nullptr) return;  // shouldn't happen
    if (g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (JNI_ATTACH_CURRENT_THREAD(g_vm, &env, nullptr) == JNI_OK) {
            didAttach = true;
        } else {
            env = nullptr;
        }
    }
}

/**
 * @brief Destroys the EnvScope and optionally detaches the thread.
 *
 * If EnvScope::didAttach is true, the destructor detaches the current thread from
 * the JVM. If false, no detachment is performed.
 */
EnvScope::~EnvScope() {
    if (didAttach && g_vm) {
        g_vm->DetachCurrentThread();
    }
}

/**
 * @brief Deliver completion callback to the JVM via NativeBridge.onNativeComplete.
 *
 * Attaches the current thread to the JVM if required, invokes the cached static
 * callback method, and detaches the thread if it was attached in this function.
 *
 * @param operationId operation identifier associated with the completion
 * @param rc completion result code (e.g. success/error/cancelled)
 * @param payload completion payload string (e.g. result or error message)
 */
void deliverCompletion(long operationId, int rc, const std::string &payload) {
    JNIEnv *env;
    bool detach = false;
    if (g_vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        JNI_ATTACH_CURRENT_THREAD(g_vm,&env, nullptr);
        detach = true;
    }

    jclass cls = g_NativeBridgeClass; // assume cached global ref
    jmethodID mid = g_onNativeComplete;
    jstring javaPayload = env->NewStringUTF(payload.c_str());

    env->CallStaticVoidMethod(cls, mid, operationId, rc, javaPayload);
    env->DeleteLocalRef(javaPayload);

    if (detach) g_vm->DetachCurrentThread();
}


#ifdef __cplusplus
}
#endif
