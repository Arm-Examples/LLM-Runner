#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.27)

set(CMAKE_CXX_STANDARD 17)

# If CMake toolchain is not defined, set it here.
if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/toolchains/native.cmake")
endif()
message(STATUS "Using CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")


# Checking if we are building Android target. Android Studio build's and CLI build's
# configuration is slightly different so we need check both ANDROID OR DEFINED ANDROID_ABI:
if(ANDROID OR DEFINED ANDROID_ABI)
    set(MIN_NDK_VERSION "29.0.14206865")
    set(_ndk_root "")
    if(DEFINED ANDROID_NDK AND ANDROID_NDK)
        set(_ndk_root "${ANDROID_NDK}")
    elseif(DEFINED ENV{NDK_PATH} AND NOT "$ENV{NDK_PATH}" STREQUAL "")
        set(_ndk_root "$ENV{NDK_PATH}")
    endif()

    if(_ndk_root STREQUAL "")
        message(FATAL_ERROR
            "Android NDK root not found, Android NDK ${MIN_NDK_VERSION}+ required. "
            "Either build with the Android toolchain (ANDROID_NDK set) or set NDK_PATH env var.")
    endif()

    if(NOT EXISTS "${_ndk_root}")
        message(FATAL_ERROR "NDK root '${_ndk_root}' does not exist.")
    endif()

    set(_ndk_version "")
    if(DEFINED ANDROID_NDK_REVISION AND NOT "${ANDROID_NDK_REVISION}" STREQUAL "")
        set(_ndk_version "${ANDROID_NDK_REVISION}")
    else()
        if(NOT EXISTS "${_ndk_root}/source.properties")
            message(FATAL_ERROR
                "NDK root '${_ndk_root}' is missing source.properties, "
                "and ANDROID_NDK_REVISION is not available. Android NDK ${MIN_NDK_VERSION}+ required.")
        endif()

        file(READ "${_ndk_root}/source.properties" _ndk_props)
        string(REGEX MATCH "Pkg\\.Revision[ \t]*=[ \t]*([0-9.]+)" _m "${_ndk_props}")
        set(_ndk_version "${CMAKE_MATCH_1}")
    endif()

    if(_ndk_version STREQUAL "")
        message(FATAL_ERROR "Failed to determine Android NDK version (root='${_ndk_root}'). "
            "Android NDK ${MIN_NDK_VERSION}+ required.")
    endif()

    message(STATUS "Android NDK detected at: ${_ndk_root}")
    message(STATUS "Android NDK revision: ${_ndk_version}")

    if(_ndk_version VERSION_LESS MIN_NDK_VERSION)
        message(FATAL_ERROR
            "Android NDK ${MIN_NDK_VERSION}+ required, found ${_ndk_version} (root='${_ndk_root}')."
        )
    endif()
endif()

# Declare project
project(arm-llm-wrapper
        VERSION     0.0.1
        DESCRIPTION "An LLM interface library"
        LANGUAGES   C CXX ASM)

set(ARM_LLM_MODULE_VERSION "${CMAKE_PROJECT_VERSION}")
set(ARM_LLM_MODULE_GIT_SHA "unknown")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}" rev-parse --short=12 HEAD
        RESULT_VARIABLE ARM_LLM_GIT_SHA_RESULT
        OUTPUT_VARIABLE ARM_LLM_GIT_SHA_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(ARM_LLM_GIT_SHA_RESULT EQUAL 0 AND NOT ARM_LLM_GIT_SHA_OUTPUT STREQUAL "")
        set(ARM_LLM_MODULE_GIT_SHA "${ARM_LLM_GIT_SHA_OUTPUT}")
    endif()
endif()
string(TIMESTAMP ARM_LLM_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

if (USE_KLEIDIAI)
    # Ensure target or host is Arm
    if (NOT CMAKE_CROSSCOMPILING)
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROC_LOWER)
        if (NOT SYSTEM_PROC_LOWER MATCHES "^(aarch64|arm64)$")
            message(WARNING
                    "KleidiAI=ON but not on an Arm host (${CMAKE_SYSTEM_PROCESSOR}).")
        endif()
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake")

# Set the runtime and lib directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/archive)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Use Release as the default build type if none was specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Set position independent code enabled
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Make coverage option available and wire flags if enabled
include(enable-coverage)

# Add default configuration options users can set from command line.
include(configuration-options)

# Download the common resources that may be used. Once these are set up
# in the repository, they won't be downloaded again for each build.
include(download-resources)

# Add sources or interface files to the build.
add_subdirectory(src)

# Add tests if they are enabled.
if(BUILD_LLM_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
