//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef ARM_LLM_WRAPPER_LOGGER_HPP
#define ARM_LLM_WRAPPER_LOGGER_HPP

#include <string>
#include <cstdarg>

namespace LlmLog {
    // formatting helper
    std::string vformat_va(const char* format, va_list args);

    inline std::string vformat(const char* format, ...) {
        va_list args;
        va_start(args, format);
        std::string s = vformat_va(format, args);
        va_end(args);
        return s;
    }
}


#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARN  1
#define LOG_LEVEL_INFO  2
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_VERBOSE 4


#ifndef ACTIVE_LOG_LEVEL
// LOG_LEVEL_ERROR is default
#define ACTIVE_LOG_LEVEL LOG_LEVEL_ERROR
#endif

// Platform-specific log macros
#ifdef __ANDROID__
    #include <android/log.h>
    #define LOG_TAG "large-language-models"
    #define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
    #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARN
        #define LOG_WARN(...)  __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
    #else
        #define LOG_WARN(...)    do { (void)0; } while(0)
    #endif
    #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO
        #define LOG_INF(...)   __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
    #else
        #define LOG_INF(...)    do { (void)0; } while(0)
    #endif
    #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
        #define LOG_DEBUG(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
    #else
        #define LOG_DEBUG(...)    do { (void)0; } while(0)
    #endif
#else
    #include <cstdio>
    #define LOG_ERROR(...) do { fprintf(stderr, "ERROR: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
    #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARN
        #define LOG_WARN(...)  do { fprintf(stderr, "WARN : "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
    #else
        #define LOG_WARN(...)    do { (void)0; } while(0)
    #endif
    #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO
        #define LOG_INF(...)   do { fprintf(stdout, "INFO : "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)
    #else
        #define LOG_INF(...)    do { (void)0; } while(0)
    #endif
    #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
        #define LOG_DEBUG(...) do { fprintf(stdout, "DEBUG: "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while(0)
    #else
        #define LOG_DEBUG(...)    do { (void)0; } while(0)
    #endif
#endif

// Exception macros using the logger
#define THROW_ERROR(fmt, ...) \
    do { \
        LOG_ERROR(fmt __VA_OPT__(, __VA_ARGS__)); \
        throw std::runtime_error(LlmLog::vformat(fmt __VA_OPT__(, __VA_ARGS__))); \
    } while (0)

#define THROW_INVALID_ARGUMENT(fmt, ...) \
    do { \
        LOG_ERROR(fmt __VA_OPT__(, __VA_ARGS__)); \
        throw std::invalid_argument(LlmLog::vformat(fmt __VA_OPT__(, __VA_ARGS__))); \
    } while (0)

#endif // ARM_LLM_WRAPPER_LOGGER_HPP
