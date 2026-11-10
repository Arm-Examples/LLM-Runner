//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "core/jni/jni_support.hpp"

#include <exception>
#include <type_traits>
#include <utility>

namespace rebar_ml::jni {

template <typename R, typename Fn>
R jni_call(JNIEnv* env, R default_value, Fn&& fn) noexcept {
    static_assert(!std::is_void_v<R>, "Use jni_call_void for void return types");
    try {
        return std::forward<Fn>(fn)();
    } catch (const std::invalid_argument& error) {
        throw_rebar_error(env, ErrorCode::invalid_argument, error.what());
        return default_value;
    } catch (const std::exception& error) {
        throw_cpp_exception(env, error);
        return default_value;
    }
}

template <typename Fn>
void jni_call_void(JNIEnv* env, Fn&& fn) noexcept {
    try {
        std::forward<Fn>(fn)();
    } catch (const std::invalid_argument& error) {
        throw_rebar_error(env, ErrorCode::invalid_argument, error.what());
    } catch (const std::exception& error) {
        throw_cpp_exception(env, error);
    }
}

}  // namespace rebar_ml::jni
