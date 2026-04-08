#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(mnn-wrapper
        DESCRIPTION "MNN wrapper interface implementation"
        LANGUAGES C CXX ASM)

# Make subprojects honor preset option() values.
# MNN uses CMake 3.6 (before CMP0077), so this policy is enabled to allow
# overwriting option() flags such as MNN_KLEIDIAI.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

include(FetchContent)
include(check-flag)

# --------------------------------------------------------------------
# MNN build
# --------------------------------------------------------------------
set(MNN_SRC_DIR "${CMAKE_BINARY_DIR}/mnn"
        CACHE PATH  "Path where mnn repo should be cloned")

set(MNN_GIT_URL "https://github.com/alibaba/MNN.git"
        CACHE STRING "Git URL for mnn repo")

set(MNN_GIT_TAG "3.3.0"
        CACHE STRING "Git tag / commit SHA for MNN repo")

# --------------------------------------------------------------------
# MNN build flags
# --------------------------------------------------------------------
set(MNN_SEP_BUILD                 OFF CACHE BOOL "Build MNN Backends and expression separately")
set(MNN_BUILD_FOR_ANDROID_COMMAND ON  CACHE BOOL "Build from command")
set(MNN_BUILD_LLM                 ON  CACHE BOOL "Build llm library based MNN.")
set(MNN_BUILD_LLM_OMNI            ON  CACHE BOOL "If build llm library, build it with omni (support image / audio)")
set(MNN_BUILD_OPENCV              ON  CACHE BOOL "Build OpenCV api in MNN for vision model")
set(MNN_IMGCODECS                 ON  CACHE BOOL "Enable imgcodecs for vision model")
set(MNN_BUILD_TEST                OFF CACHE BOOL "Build tests or not")
set(MNN_BUILD_TOOLS               OFF CACHE BOOL "Build tools/cpp or not")
set(MNN_BUILD_BENCHMARK           OFF CACHE BOOL "Build MNN benchmark  or not (separate from llm_bench)")

# KleidiAI configuration
if(DEFINED MNN_KLEIDIAI)
    message(FATAL_ERROR "Don't use framework specific KleidiAI flag, configure 'USE_KLEIDIAI' flag instead.")
else()
    set_kleidiai_flag()
    set(MNN_KLEIDIAI ${USE_KLEIDIAI})
endif()

FetchContent_Declare(mnn
  GIT_REPOSITORY ${MNN_GIT_URL}
  GIT_TAG        ${MNN_GIT_TAG}
  GIT_SHALLOW    1
  SOURCE_DIR     ${MNN_SRC_DIR}
  EXCLUDE_FROM_ALL)

FetchContent_MakeAvailable(mnn)

# Build llm_bench and llm_demo standalone application
if(BUILD_BENCHMARK)
    set_target_properties(llm_bench PROPERTIES EXCLUDE_FROM_ALL OFF)
    set_target_properties(llm_demo PROPERTIES EXCLUDE_FROM_ALL OFF)
endif()

if (NOT TARGET llm-config)
    message(FATAL_ERROR "llm-config target not defined")
elseif (NOT TARGET llm-chat)
    message(FATAL_ERROR "llm-chat target not defined")
elseif (NOT TARGET llm-interface)
    message(FATAL_ERROR "llm-interface target not defined")
endif()

add_library(llm-framework STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/MnnImpl.cpp LlmFactory.cpp)

target_include_directories(llm-framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(llm-framework
  PUBLIC
    "${MNN_SRC_DIR}/include"
    "${MNN_SRC_DIR}/transformers/llm/engine/include"
)

# List all libraries that we need to depend on here:
target_link_libraries(llm-framework PUBLIC
    MNN
    llm-config
    llm-chat
    llm-interface)
