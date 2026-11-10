//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "core/jni/rebar_jni_conversions.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace rebar_ml::jni {

namespace {

struct ReceiveResultJniCache {
    jclass text_receive_result_class = nullptr;
    jmethodID text_receive_result_ctor = nullptr;
    jclass audio_receive_result_class = nullptr;
    jmethodID audio_receive_result_ctor = nullptr;
};

ReceiveResultJniCache& receive_result_jni_cache() {
    static ReceiveResultJniCache cache;
    return cache;
}

std::once_flag g_receive_result_cache_once;

[[noreturn]] void throw_jni_cache_init_error(JNIEnv* env, const char* message) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    throw std::runtime_error(message);
}

void init_receive_result_cache(JNIEnv* env) {
    if (!env) {
        throw std::runtime_error("JNI cache init failed: JNIEnv is null");
    }

    jclass local_text_class = env->FindClass("com/arm/rebarml/NativeTextReceiveResult");
    if (!local_text_class) {
        throw_jni_cache_init_error(env, "JNI cache init failed: NativeTextReceiveResult class not found");
    }
    jclass text_class_global =
        reinterpret_cast<jclass>(env->NewGlobalRef(local_text_class));
    env->DeleteLocalRef(local_text_class);
    if (!text_class_global) {
        throw_jni_cache_init_error(env, "JNI cache init failed: NativeTextReceiveResult global ref allocation failed");
    }
    jmethodID text_ctor = env->GetMethodID(
        text_class_global,
        "<init>",
        "(ILjava/lang/String;Ljava/lang/String;ZILjava/lang/String;)V");
    if (!text_ctor) {
        env->DeleteGlobalRef(text_class_global);
        throw_jni_cache_init_error(env, "JNI cache init failed: NativeTextReceiveResult constructor not found");
    }

    jclass local_audio_class = env->FindClass("com/arm/rebarml/NativeAudioReceiveResult");
    if (!local_audio_class) {
        env->DeleteGlobalRef(text_class_global);
        throw_jni_cache_init_error(env, "JNI cache init failed: NativeAudioReceiveResult class not found");
    }
    jclass audio_class_global =
        reinterpret_cast<jclass>(env->NewGlobalRef(local_audio_class));
    env->DeleteLocalRef(local_audio_class);
    if (!audio_class_global) {
        env->DeleteGlobalRef(text_class_global);
        throw_jni_cache_init_error(env, "JNI cache init failed: NativeAudioReceiveResult global ref allocation failed");
    }
    jmethodID audio_ctor = env->GetMethodID(
        audio_class_global,
        "<init>",
        "(I[BIIILjava/lang/String;ZILjava/lang/String;)V");
    if (!audio_ctor) {
        env->DeleteGlobalRef(audio_class_global);
        env->DeleteGlobalRef(text_class_global);
        throw_jni_cache_init_error(env, "JNI cache init failed: NativeAudioReceiveResult constructor not found");
    }

    auto& cache = receive_result_jni_cache();
    cache.text_receive_result_class = text_class_global;
    cache.text_receive_result_ctor = text_ctor;
    cache.audio_receive_result_class = audio_class_global;
    cache.audio_receive_result_ctor = audio_ctor;
}

const ReceiveResultJniCache& require_receive_result_cache(JNIEnv* env) {
    std::call_once(g_receive_result_cache_once, [&] { init_receive_result_cache(env); });
    return receive_result_jni_cache();
}

}  // namespace

ConnectionOptions connection_options_from_jni(
    jint queue_capacity,
    jint overflow_policy) {
    if (queue_capacity <= 0 || overflow_policy < 0 || overflow_policy > 3) {
        throw std::invalid_argument("connection options are invalid");
    }
    return ConnectionOptions{
        static_cast<std::size_t>(queue_capacity),
        static_cast<OverflowPolicy>(overflow_policy),
    };
}

AudioData make_audio_data(
    JNIEnv* env,
    jobject byte_buffer,
    jint offset,
    jint length,
    jint codec,
    jint sample_rate,
    jint channels,
    jstring language) {
    if (!byte_buffer) throw std::invalid_argument("audio ByteBuffer is null");
    auto* base = static_cast<std::byte*>(
        env->GetDirectBufferAddress(byte_buffer));
    const jlong capacity = env->GetDirectBufferCapacity(byte_buffer);
    if (!base || capacity < 0) {
        throw std::invalid_argument("audio ByteBuffer must be direct");
    }
    if (offset < 0 || length < 0 ||
        static_cast<jlong>(offset) + length > capacity) {
        throw std::invalid_argument("audio ByteBuffer range is invalid");
    }
    if (codec < 0 || codec > static_cast<jint>(AudioCodec::pcm_f32le) || sample_rate < 0 ||
        channels < 0 || channels > 65535) {
        throw std::invalid_argument("audio format is invalid");
    }

    auto bytes = std::make_shared<std::vector<std::byte>>(
        static_cast<std::size_t>(length));
    std::memcpy(
        bytes->data(), base + offset, static_cast<std::size_t>(length));
    return AudioData{
        bytes,
        AudioFormat{
            static_cast<AudioCodec>(codec),
            static_cast<std::uint32_t>(sample_rate),
            static_cast<std::uint16_t>(channels),
        },
        from_jstring(env, language),
    };
}

jobject make_text_receive_result(
    JNIEnv* env,
    const OutputRetrieval<TextData>& result) {
    const auto& cache = require_receive_result_cache(env);

    jint status = 0;
    jstring text = nullptr;
    jstring language = nullptr;
    jboolean final_value = JNI_TRUE;
    jint error_code = 0;
    jstring error_message = nullptr;

    if (result.status == RetrievalStatus::cancelled) {
        status = 2;
    } else if (result.status == RetrievalStatus::error) {
        status = 1;
        if (result.error) {
            error_code = static_cast<jint>(result.error->code);
            error_message = to_jstring(env, result.error->message);
        }
    } else if (result.item) {
        text = to_jstring(env, result.item->data.text);
        language = to_optional_jstring(env, result.item->data.language);
        final_value = result.item->final ? JNI_TRUE : JNI_FALSE;
    }

    jobject object = env->NewObject(
        cache.text_receive_result_class,
        cache.text_receive_result_ctor,
        status,
        text,
        language,
        final_value,
        error_code,
        error_message);
    if (text) env->DeleteLocalRef(text);
    if (language) env->DeleteLocalRef(language);
    if (error_message) env->DeleteLocalRef(error_message);
    return object;
}

jobject make_audio_receive_result(
    JNIEnv* env,
    const OutputRetrieval<AudioData>& result) {
    const auto& cache = require_receive_result_cache(env);

    jint status = 0;
    jbyteArray bytes = nullptr;
    jint codec = 0;
    jint sample_rate = 0;
    jint channels = 0;
    jstring language = nullptr;
    jboolean final_value = JNI_TRUE;
    jint error_code = 0;
    jstring error_message = nullptr;

    if (result.status == RetrievalStatus::cancelled) {
        status = 2;
    } else if (result.status == RetrievalStatus::error) {
        status = 1;
        if (result.error) {
            error_code = static_cast<jint>(result.error->code);
            error_message = to_jstring(env, result.error->message);
        }
    } else if (result.item) {
        const auto& audio = result.item->data;
        codec = static_cast<jint>(audio.format.codec);
        sample_rate = static_cast<jint>(audio.format.sample_rate_hz);
        channels = static_cast<jint>(audio.format.channels);
        language = to_optional_jstring(env, audio.language);
        final_value = result.item->final ? JNI_TRUE : JNI_FALSE;

        const jsize size = audio.bytes
            ? static_cast<jsize>(audio.bytes->size())
            : 0;
        bytes = env->NewByteArray(size);
        if (bytes && size > 0) {
            // MIGRATION TODO: replace this copy with a direct ByteBuffer plus
            // native lease if the project's audio player supports it.
            env->SetByteArrayRegion(
                bytes,
                0,
                size,
                reinterpret_cast<const jbyte*>(audio.bytes->data()));
        }
    }

    jobject object = env->NewObject(
        cache.audio_receive_result_class,
        cache.audio_receive_result_ctor,
        status,
        bytes,
        codec,
        sample_rate,
        channels,
        language,
        final_value,
        error_code,
        error_message);
    if (bytes) env->DeleteLocalRef(bytes);
    if (language) env->DeleteLocalRef(language);
    if (error_message) env->DeleteLocalRef(error_message);
    return object;
}

}  // namespace rebar_ml::jni
