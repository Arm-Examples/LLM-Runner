#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

set(LLM_FRAMEWORK "llama.cpp" CACHE STRING
    "Dependency name to configure the project for")

# Available options:
set(CACHE LLM_FRAMEWORK PROPERTY STRINGS
    "llama.cpp"
    "onnxruntime-genai"
    "mediapipe"
    "mnn")

set(DOWNLOADS_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/resources_downloaded
    CACHE STRING
    "Directory where required resources are downloaded into")

set(DOWNLOADS_LOCK_TIMEOUT 600
    CACHE STRING
    "Timeout in seconds for lock to hold off concurrent CMake configurations
    trying to download resources to the same directory.")

option(BUILD_BENCHMARK    "Build benchmark binary"            ON)
option(BUILD_LLM_TESTING  "Build unit tests"                  ON)
option(BUILD_JNI_LIB      "Build JNI lib"                     ON)
option(LLM_JNI_TIMING     "Enable JNI timing helpers"         OFF)
option(LLAMA_BUILD_COMMON "Include LLAMA common"              ON)
option(ENABLE_STREAMLINE  "Enable Arm Streamline annotations" OFF)
