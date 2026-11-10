//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "core/jni/jni_call.hpp"
#include "core/jni/rebar_jni_conversions.hpp"

namespace {

using rebar_ml::jni::connection_options_from_jni;
using rebar_ml::jni::jni_call;
using rebar_ml::jni::jni_call_void;
using rebar_ml::jni::make_audio_data;
using rebar_ml::jni::make_audio_receive_result;
using rebar_ml::jni::make_handle;
using rebar_ml::jni::make_text_receive_result;
using rebar_ml::jni::release_handle;
using rebar_ml::jni::require_handle;
using rebar_ml::jni::unwrap;
using rebar_ml::jni::unwrap_void;

}  // namespace

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseNode(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::INode>(handle);
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getNodeAudioInputPort(
    JNIEnv* env, jclass, jlong node_handle, jint index) {
    return jni_call<jlong>(env, 0, [&] {
        return make_handle<rebar_ml::IAudioInputPort>(
            require_handle<rebar_ml::INode>(node_handle)->get_input_port<rebar_ml::AudioData>(index));
    });
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getNodeAudioOutputPort(
    JNIEnv* env, jclass, jlong node_handle, jint index) {
    return jni_call<jlong>(env, 0, [&] {
        return make_handle<rebar_ml::IAudioOutputPort>(
            require_handle<rebar_ml::INode>(node_handle)->get_output_port<rebar_ml::AudioData>(index));
    });
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getNodeTextInputPort(
    JNIEnv* env, jclass, jlong node_handle, jint index) {
    return jni_call<jlong>(env, 0, [&] {
        return make_handle<rebar_ml::ITextInputPort>(
            require_handle<rebar_ml::INode>(node_handle)->get_input_port<rebar_ml::TextData>(index));
    });
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getNodeTextOutputPort(
    JNIEnv* env, jclass, jlong node_handle, jint index) {
    return jni_call<jlong>(env, 0, [&] {
        return make_handle<rebar_ml::ITextOutputPort>(
            require_handle<rebar_ml::INode>(node_handle)->get_output_port<rebar_ml::TextData>(index));
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseAudioInputPort(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::IAudioInputPort>(handle);
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseAudioOutputPort(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::IAudioOutputPort>(handle);
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseTextInputPort(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::ITextInputPort>(handle);
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseTextOutputPort(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::ITextOutputPort>(handle);
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_connectAudioPorts(
    JNIEnv* env,
    jclass,
    jlong output_handle,
    jlong input_handle,
    jint queue_capacity,
    jint overflow_policy) {
    jni_call_void(env, [&] {
        unwrap_void(
            env,
            require_handle<rebar_ml::IAudioOutputPort>(output_handle)->connect(
                require_handle<rebar_ml::IAudioInputPort>(input_handle),
                connection_options_from_jni(queue_capacity, overflow_policy)));
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_connectTextPorts(
    JNIEnv* env,
    jclass,
    jlong output_handle,
    jlong input_handle,
    jint queue_capacity,
    jint overflow_policy) {
    jni_call_void(env, [&] {
        unwrap_void(
            env,
            require_handle<rebar_ml::ITextOutputPort>(output_handle)->connect(
                require_handle<rebar_ml::ITextInputPort>(input_handle),
                connection_options_from_jni(queue_capacity, overflow_policy)));
    });
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_createPipelineSession(
    JNIEnv* env, jclass, jlong node_handle) {
    return jni_call<jlong>(env, 0, [&] {
        std::shared_ptr<rebar_ml::IPipelineSession> pipeline_session;
        if (!unwrap(
                env,
                require_handle<rebar_ml::INode>(node_handle)->create_pipeline_session(),
                pipeline_session)) {
            return static_cast<jlong>(0);
        }
        return make_handle<rebar_ml::IPipelineSession>(std::move(pipeline_session));
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releasePipelineSession(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::IPipelineSession>(handle);
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getPipelineSessionNodeSession(
    JNIEnv* env,
    jclass,
    jlong pipeline_session_handle,
    jlong node_handle) {
    return jni_call<jlong>(env, 0, [&] {
        std::shared_ptr<rebar_ml::INodeSession> session;
        if (!unwrap(
                env,
                require_handle<rebar_ml::IPipelineSession>(pipeline_session_handle)
                    ->get_node_session(require_handle<rebar_ml::INode>(node_handle)),
                session)) {
            return static_cast<jlong>(0);
        }
        return make_handle<rebar_ml::INodeSession>(std::move(session));
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_submitPipelineSession(
    JNIEnv* env, jclass, jlong pipeline_session_handle) {
    jni_call_void(env, [&] {
        unwrap_void(
            env,
            require_handle<rebar_ml::IPipelineSession>(pipeline_session_handle)->submit());
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_finishPipelineSessionInputs(
    JNIEnv* env, jclass, jlong pipeline_session_handle) {
    jni_call_void(env, [&] {
        unwrap_void(
            env,
            require_handle<rebar_ml::IPipelineSession>(pipeline_session_handle)->finish_inputs());
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_teardownPipelineSession(
    JNIEnv* env, jclass, jlong pipeline_session_handle) {
    jni_call_void(env, [&] {
        require_handle<rebar_ml::IPipelineSession>(pipeline_session_handle)->teardown();
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_cancelPipelineSession(
    JNIEnv* env, jclass, jlong pipeline_session_handle) {
    jni_call_void(env, [&] {
        require_handle<rebar_ml::IPipelineSession>(pipeline_session_handle)->cancel();
    });
}

JNIEXPORT jboolean JNICALL
Java_com_arm_rebarml_RebarML_isPipelineSessionCancelled(
    JNIEnv* env, jclass, jlong pipeline_session_handle) {
    return jni_call<jboolean>(env, JNI_FALSE, [&] {
        return require_handle<rebar_ml::IPipelineSession>(pipeline_session_handle)->is_cancelled()
            ? JNI_TRUE
            : JNI_FALSE;
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseNodeSession(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::INodeSession>(handle);
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getNodeSessionAudioInput(
    JNIEnv* env, jclass, jlong session_handle, jint index) {
    return jni_call<jlong>(env, 0, [&] {
        return make_handle<rebar_ml::IAudioInput>(
            require_handle<rebar_ml::INodeSession>(session_handle)->get_input<rebar_ml::AudioData>(index));
    });
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getNodeSessionTextOutput(
    JNIEnv* env, jclass, jlong session_handle, jint index) {
    return jni_call<jlong>(env, 0, [&] {
        return make_handle<rebar_ml::ITextOutput>(
            require_handle<rebar_ml::INodeSession>(session_handle)->get_output<rebar_ml::TextData>(index));
    });
}

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_getNodeSessionAudioOutput(
    JNIEnv* env, jclass, jlong session_handle, jint index) {
    return jni_call<jlong>(env, 0, [&] {
        return make_handle<rebar_ml::IAudioOutput>(
            require_handle<rebar_ml::INodeSession>(session_handle)->get_output<rebar_ml::AudioData>(index));
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseAudioInput(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::IAudioInput>(handle);
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_audioInputAdd(
    JNIEnv* env,
    jclass,
    jlong handle,
    jobject bytes,
    jint offset,
    jint length,
    jint codec,
    jint sample_rate,
    jint channels,
    jstring language) {
    jni_call_void(env, [&] {
        auto data = make_audio_data(
            env,
            bytes,
            offset,
            length,
            codec,
            sample_rate,
            channels,
            language);
        unwrap_void(env, require_handle<rebar_ml::IAudioInput>(handle)->add(std::move(data)));
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_audioInputSend(
    JNIEnv* env,
    jclass,
    jlong handle,
    jobject bytes,
    jint offset,
    jint length,
    jint codec,
    jint sample_rate,
    jint channels,
    jstring language) {
    jni_call_void(env, [&] {
        auto data = make_audio_data(
            env,
            bytes,
            offset,
            length,
            codec,
            sample_rate,
            channels,
            language);
        unwrap_void(env, require_handle<rebar_ml::IAudioInput>(handle)->send(std::move(data)));
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseTextOutput(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::ITextOutput>(handle);
}

JNIEXPORT jobject JNICALL
Java_com_arm_rebarml_RebarML_textOutputReceive(
    JNIEnv* env, jclass, jlong handle) {
    return jni_call<jobject>(env, nullptr, [&] {
        return make_text_receive_result(
            env,
            require_handle<rebar_ml::ITextOutput>(handle)->receive());
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_textOutputCancel(
    JNIEnv* env, jclass, jlong handle) {
    jni_call_void(env, [&] {
        require_handle<rebar_ml::ITextOutput>(handle)->cancel();
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_releaseAudioOutput(
    JNIEnv*, jclass, jlong handle) {
    release_handle<rebar_ml::IAudioOutput>(handle);
}

JNIEXPORT jobject JNICALL
Java_com_arm_rebarml_RebarML_audioOutputReceive(
    JNIEnv* env, jclass, jlong handle) {
    return jni_call<jobject>(env, nullptr, [&] {
        return make_audio_receive_result(
            env,
            require_handle<rebar_ml::IAudioOutput>(handle)->receive());
    });
}

JNIEXPORT void JNICALL
Java_com_arm_rebarml_RebarML_audioOutputCancel(
    JNIEnv* env, jclass, jlong handle) {
    jni_call_void(env, [&] {
        require_handle<rebar_ml::IAudioOutput>(handle)->cancel();
    });
}

}  // extern "C"
