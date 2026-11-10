//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "core/jni/jni_support.hpp"

#include <jni.h>

namespace rebar_ml::jni {

ConnectionOptions connection_options_from_jni(jint queue_capacity, jint overflow_policy);

AudioData make_audio_data(
    JNIEnv* env,
    jobject byte_buffer,
    jint offset,
    jint length,
    jint codec,
    jint sample_rate,
    jint channels,
    jstring language);

jobject make_text_receive_result(
    JNIEnv* env,
    const OutputRetrieval<TextData>& result);

jobject make_audio_receive_result(
    JNIEnv* env,
    const OutputRetrieval<AudioData>& result);

}  // namespace rebar_ml::jni
