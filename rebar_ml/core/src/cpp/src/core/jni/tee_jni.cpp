//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "core/jni/jni_support.hpp"

#include "rebar/tee.hpp"

#include <cstddef>
#include <stdexcept>

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_arm_rebarml_RebarML_createTeeNode(
    JNIEnv* env, jclass, jint output_count) {
    try {
        if (output_count <= 0) {
            throw std::invalid_argument("output count must be greater than zero");
        }
        return rebar_ml::jni::make_handle<rebar_ml::INode>(
            rebar_ml::make_tee_node(rebar_ml::TeeOptions{
                static_cast<std::size_t>(output_count),
            }));
    } catch (const std::invalid_argument& error) {
        rebar_ml::jni::throw_rebar_error(env, rebar_ml::ErrorCode::invalid_argument, error.what());
        return 0;
    } catch (const std::exception& error) {
        rebar_ml::jni::throw_cpp_exception(env, error);
        return 0;
    }
}

}  // extern "C"
