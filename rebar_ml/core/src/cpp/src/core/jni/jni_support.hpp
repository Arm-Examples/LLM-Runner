//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <jni.h>

#include "rebar/rebar.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rebar_ml::jni {

template <typename T>
jlong make_handle(std::shared_ptr<T> value) {
    if (!value) return 0;
    return reinterpret_cast<jlong>(new std::shared_ptr<T>(std::move(value)));
}

template <typename T>
std::shared_ptr<T>& require_handle(jlong value) {
    if (value == 0) throw std::invalid_argument("native handle is closed");
    return *reinterpret_cast<std::shared_ptr<T>*>(value);
}

template <typename T>
void release_handle(jlong value) {
    delete reinterpret_cast<std::shared_ptr<T>*>(value);
}

// Encoding contract across the JNI boundary:
// - Java/Kotlin strings are UTF-16 code units.
// - Native strings are standard UTF-8 (RFC 3629).
// Unpaired UTF-16 surrogates are replaced with U+FFFD when converting to UTF-8.
inline std::string utf16_to_utf8(std::u16string_view input) {
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        std::uint32_t codepoint = input[i];

        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (i + 1 < input.size()) {
                const std::uint32_t low = input[i + 1];
                if (low >= 0xdc00 && low <= 0xdfff) {
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10U) + (low - 0xdc00);
                    ++i;
                } else {
                    codepoint = 0xfffd;
                }
            } else {
                codepoint = 0xfffd;
            }
        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
            codepoint = 0xfffd;
        }

        if (codepoint <= 0x7f) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (codepoint >> 6U)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3fU)));
        } else if (codepoint <= 0xffff) {
            output.push_back(static_cast<char>(0xe0 | (codepoint >> 12U)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3fU)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (codepoint >> 18U)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3fU)));
        }
    }
    return output;
}

inline std::optional<std::string> from_jstring(JNIEnv* env, jstring value) {
    if (!value) return std::nullopt;
    const jsize length = env->GetStringLength(value);
    const jchar* chars = env->GetStringChars(value, nullptr);
    if (!chars) return std::nullopt;
    struct JStringCharsGuard {
        JNIEnv* env;
        jstring value;
        const jchar* chars;
        ~JStringCharsGuard() { env->ReleaseStringChars(value, chars); }
    } guard{env, value, chars};
    static_assert(sizeof(jchar) == sizeof(char16_t));
    auto utf16 = std::u16string_view{
        reinterpret_cast<const char16_t*>(chars),
        static_cast<std::size_t>(length),
    };
    return utf16_to_utf8(utf16);
}

inline std::u16string utf8_to_utf16(std::string_view input) {
    std::u16string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size();) {
        std::uint32_t codepoint = 0;
        const auto first = static_cast<unsigned char>(input[i]);
        std::size_t count = 0;
        if (first < 0x80) {
            codepoint = first;
            count = 1;
        } else if ((first & 0xe0) == 0xc0 && i + 1 < input.size()) {
            codepoint = first & 0x1f;
            count = 2;
        } else if ((first & 0xf0) == 0xe0 && i + 2 < input.size()) {
            codepoint = first & 0x0f;
            count = 3;
        } else if ((first & 0xf8) == 0xf0 && i + 3 < input.size()) {
            codepoint = first & 0x07;
            count = 4;
        } else {
            codepoint = 0xfffd;
            count = 1;
        }

        bool valid = true;
        for (std::size_t j = 1; j < count; ++j) {
            const auto next = static_cast<unsigned char>(input[i + j]);
            if ((next & 0xc0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6U) | (next & 0x3fU);
        }
        if (!valid || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            codepoint = 0xfffd;
            count = 1;
        }

        if (codepoint <= 0xffff) {
            output.push_back(static_cast<char16_t>(codepoint));
        } else {
            codepoint -= 0x10000;
            output.push_back(static_cast<char16_t>(0xd800 + (codepoint >> 10U)));
            output.push_back(static_cast<char16_t>(0xdc00 + (codepoint & 0x3ffU)));
        }
        i += count;
    }
    return output;
}

inline jstring to_jstring(JNIEnv* env, std::string_view value) {
    auto utf16 = utf8_to_utf16(value);
    return env->NewString(
        reinterpret_cast<const jchar*>(utf16.data()),
        static_cast<jsize>(utf16.size()));
}

inline jstring to_optional_jstring(
    JNIEnv* env,
    const std::optional<std::string>& value) {
    return value ? to_jstring(env, *value) : nullptr;
}

inline void throw_rebar_error(JNIEnv* env, const Error& error) {
    jclass clazz = env->FindClass("com/arm/rebarml/RebarException");
    if (!clazz) return;
    jmethodID ctor = env->GetMethodID(clazz, "<init>", "(ILjava/lang/String;)V");
    if (!ctor) return;
    jstring message = to_jstring(env, error.message);
    jobject exception = env->NewObject(
        clazz, ctor, static_cast<jint>(error.code), message);
    if (message) env->DeleteLocalRef(message);
    if (exception) env->Throw(static_cast<jthrowable>(exception));
}

inline void throw_rebar_error(
    JNIEnv* env,
    ErrorCode code,
    std::string_view message) {
    throw_rebar_error(env, Error{code, std::string(message)});
}

inline void throw_cpp_exception(JNIEnv* env, const std::exception& error) {
    throw_rebar_error(env, Error{ErrorCode::backend_failure, error.what()});
}

template <typename T>
bool unwrap(JNIEnv* env, const Result<T>& result, T& output) {
    if (!result) {
        throw_rebar_error(env, result.error());
        return false;
    }
    output = *result;
    return true;
}

inline bool unwrap_void(JNIEnv* env, const Result<void>& result) {
    if (!result) {
        throw_rebar_error(env, result.error());
        return false;
    }
    return true;
}

}  // namespace rebar_ml::jni
