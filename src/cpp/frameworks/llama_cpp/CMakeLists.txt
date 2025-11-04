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
include(check-flag)

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

set(LLAMA_GIT_SHA "4258e0cfe72c72ad2787be1e9f93253e89219455"
        CACHE STRING
        "Git commit SHA for llama.cpp repo")


set(LLAMA_BUILD_TOOLS    ON)
set(LLAMA_CURL           OFF CACHE BOOL "llama: use libcurl to download model from an URL")


# KleidiAI configuration
if(DEFINED GGML_CPU_KLEIDIAI)
    message(FATAL_ERROR "Don't use framework specific KleidiAI flag, configure 'USE_KLEIDIAI' flag instead.")
else()
    set_kleidiai_flag()
    set(GGML_CPU_KLEIDIAI  ${USE_KLEIDIAI})
endif()

set(GGML_BUILD_NUMBER 1) # We do a shallow clone `--depth=1`

# Fetch the dependency Git repo here
FetchContent_Declare(llama-cpp
    GIT_REPOSITORY ${LLAMA_GIT_URL}
    GIT_TAG ${LLAMA_GIT_SHA}
    GIT_SHALLOW ${GGML_BUILD_NUMBER}
    SOURCE_DIR ${LLAMA_SRC_DIR}
    EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(llama-cpp)

# Build llama-bench binary
if(BUILD_BENCHMARK)
    set_target_properties(llama-bench PROPERTIES EXCLUDE_FROM_ALL OFF)
endif()

# For Android we build the variant arch backend shared libraries
if(DEFINED ANDROID_ABI)

    # The following block of code ensures that BUILD_SHARED_LIBS defaults to ON for android builds.
    # If the BUILD_SHARED_LIBS is OFF via the CLI / Cache the build failed with an error message
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS is required for android builds" ON)
    if(NOT BUILD_SHARED_LIBS)
        message(FATAL_ERROR
                "BUILD_SHARED_LIBS must be ON for android. You passed -DBUILD_SHARED_LIBS=OFF or it is cached as OFF.\n"
                "Fix by re-configuring with -DBUILD_SHARED_LIBS=ON and/or deleting entry from cache.")
    endif()

    set(GGML_BACKEND_DL ON)
    set(GGML_CPU_ALL_VARIANTS ON)

    include(llama-cpp-workaround)
endif()

# Check internally defined dependency targets are visible
if (NOT TARGET arm-llm-config)
    message(FATAL_ERROR "arm-llm-config target not defined")
elseif (NOT TARGET arm-llm-chat)
    message(FATAL_ERROR "arm-llm-chat target not defined")
elseif (NOT TARGET arm-llm-interface)
    message(FATAL_ERROR "arm-llm-interface target not defined")
elseif (NOT TARGET arm-llm-log)
    message(FATAL_ERROR "arm-llm-log target not defined")
endif()

add_library(arm-llm-framework STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/LlamaTextImpl.cpp LlamaVisionImpl.cpp LlmFactory.cpp)

target_include_directories(arm-llm-framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

# List all libraries that we need to depend on here:
target_link_libraries(arm-llm-framework PUBLIC
    mtmd
    llama
    common
    arm-llm-chat
    arm-llm-config
    arm-llm-interface
    arm-llm-log )
