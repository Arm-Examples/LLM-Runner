#
# SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(llama-cpp-wrapper
        VERSION 0.0.1
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

# Identify system processor for Arm
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" lower_system_processor)
if (lower_system_processor MATCHES "^(aarch64|arm64)$")
    set(ARM_SYSTEM_PROCESSOR ON BOOL)
endif()

# If the user has NOT explicitly set GGML_CPU_KLEIDIAI
if (NOT DEFINED GGML_CPU_KLEIDIAI)
    # if we are on arm64/aarch64, then default KleidiAI to ON.
    if (ARM_SYSTEM_PROCESSOR)
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

# Avoid enabling KleidiAI for native builds
if(GGML_CPU_KLEIDIAI AND NOT ARM_SYSTEM_PROCESSOR)
    message(FATAL_ERROR "KleidiAI enabled, but not supported on ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# -----------------------------------------
# NOTE: Current work flow for i8mm implementation:
# If user provides any -march flag, check if compiler supports it.
# -----------------------------------------

# Include functions for flag checking
include(check-flag)

# Define the list of supported languages.
set(SUPPORTED_LANGUAGES C CXX)

# Check and set C,CXX flags
foreach(LANG ${SUPPORTED_LANGUAGES})
    # Check for -march flag in C flags.
    string(REGEX MATCH "-march=[^ ]+" USER_${LANG}_MARCH_FLAG "${CMAKE_${LANG}_FLAGS}")
    if (USER_${LANG}_MARCH_FLAG)
        message(STATUS "User provided -march flag in ${LANG} flags: ${USER_${LANG}_MARCH_FLAG}")
        # Check if the compiler supports the provided C,CXX -march flag
        check_compiler_support("${LANG}" "${USER_${LANG}_MARCH_FLAG}")
    endif()
endforeach()

# Fetch the dependency Git repo here
FetchContent_Declare(llama-cpp
        GIT_REPOSITORY ${LLAMA_GIT_URL}
        GIT_TAG ${LLAMA_GIT_SHA}
        GIT_SHALLOW 1 # We only need shallow clone with `--depth=1`
        SOURCE_DIR ${LLAMA_SRC_DIR}
        )

FetchContent_MakeAvailable(llama-cpp)

if (NOT TARGET arm-llm-cpp)
    add_library(arm-llm-cpp INTERFACE)
    target_include_directories(arm-llm-cpp INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include)
endif ()

# Add the current
target_include_directories(arm-llm-cpp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Do not turn incompatible pointer types warning into an error
# Temporary fix for ggml-cpu target for the revision of llama.cpp used currently
target_compile_options(ggml-cpu PRIVATE -Wno-error=incompatible-pointer-types)

# List all libraries that we need to depend on here:
target_link_libraries(arm-llm-cpp INTERFACE
        llama
        common
)

if (BUILD_JNI_LIB)
    # Make sure JNI include directories have been set.
    include(find-jni)

    add_library(arm-llm-jni SHARED
            ${CMAKE_CURRENT_SOURCE_DIR}/jni/Llama.cpp)

    target_link_libraries(arm-llm-jni PUBLIC arm-llm-cpp)
    target_include_directories(arm-llm-jni PUBLIC
            ${JNI_INCLUDE_DIRS} # Populated by FindJNI CMake module
    )
endif ()
