#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(llama-cpp-wrapper
        DESCRIPTION "llama.cpp wrapper interface implementation"
        LANGUAGES C CXX ASM)

include(FetchContent)

# Where should llama.cpp sources be cloned into?
# It might make sense to download sources into resources as well and not
# every time into the CMake binary directory. However, because we currently
# need to patch it, and possibly do it conditionally based on target type
# we have this arrangement for the time being.
set(LLAMA_SRC_DIR "${CMAKE_BINARY_DIR}/llama.cpp"
        CACHE PATH
        "Path where llama.cpp repo should be cloned into")

set(LLAMA_GIT_URL "https://github.com/ggerganov/llama.cpp.git"
        CACHE STRING
        "Git URL for llama.cpp repo")

set(LLAMA_GIT_SHA "a4090d1"
        CACHE STRING
        "Git commit SHA for llama.cpp repo")

set(LLAMA_BUILD_EXAMPLES ${BUILD_EXECUTABLE} CACHE BOOL "Build llama.cpp examples")

# If the user has NOT explicitly set GGML_CPU_KLEIDIAI
if (NOT DEFINED GGML_CPU_KLEIDIAI)
    # if we are on arm64/aarch64, then default KleidiAI to ON.
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(GGML_CPU_KLEIDIAI ON CACHE BOOL
            "Enable KleidiAI by default on ${CMAKE_SYSTEM_PROCESSOR}")
        message(STATUS "KleidiAI enabled by default")
    # if we are NOT on arm64/aarch64, then default KleidiAI to OFF.
    else()
        set(GGML_CPU_KLEIDIAI OFF CACHE BOOL
            "Disable KleidiAI by default on ${CMAKE_SYSTEM_PROCESSOR}")
        message(STATUS "KleidiAI disabled by default")
    endif()
else ()
    message(STATUS "KleidiAI: ${GGML_CPU_KLEIDIAI}")
endif()

set(GGML_BUILD_NUMBER 1) # We do a shallow clone `--depth=1`

# Fetch the dependency Git repo here
FetchContent_Declare(llama-cpp
    GIT_REPOSITORY ${LLAMA_GIT_URL}
    GIT_TAG ${LLAMA_GIT_SHA}
    GIT_SHALLOW ${GGML_BUILD_NUMBER}
    SOURCE_DIR ${LLAMA_SRC_DIR})

FetchContent_MakeAvailable(llama-cpp)

# Check internally defined dependency targets are visible
if (NOT TARGET arm-llm-config)
    message(FATAL_ERROR "arm-llm-config target not defined")
elseif (NOT TARGET arm-llm-interface)
    message(FATAL_ERROR "arm-llm-interface target not defined")
endif()

add_library(arm-llm-framework STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/LlamaImpl.cpp)

target_include_directories(arm-llm-framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

# List all libraries that we need to depend on here:
target_link_libraries(arm-llm-framework PUBLIC
    llama
    common
    arm-llm-config
    arm-llm-interface)
