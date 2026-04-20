#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# We default to share libs on for llama.cpp on android.
# If the user attempts to configure this target with BUILD_SHARED_LIBS OFF
# a fatal error will be thrown in llama_cpp/CMakeLists.txt.
if (${LLM_FRAMEWORK} STREQUAL "llama.cpp" AND DEFINED ANDROID_ABI)
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" ON)
else()
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" OFF)
endif()
message (STATUS "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

# Pull in LLM framework library
if (${LLM_FRAMEWORK} STREQUAL "llama.cpp")
    add_subdirectory(llama_cpp)
    set(ARM_LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS "llama.cpp=${LLAMA_GIT_SHA}" PARENT_SCOPE)
elseif(${LLM_FRAMEWORK} STREQUAL "onnxruntime-genai")
    add_subdirectory(onnxruntime_genai)
    set(ARM_LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS
        "onnxruntime=${ONNXRUNTIME_GIT_TAG}, onnxruntime-genai=${ONNXRT_GENAI_GIT_TAG}" PARENT_SCOPE)
elseif(${LLM_FRAMEWORK} STREQUAL "mnn")
    add_subdirectory(mnn)
    set(ARM_LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS "mnn=${MNN_GIT_TAG}" PARENT_SCOPE)
elseif(${LLM_FRAMEWORK} STREQUAL "mediapipe")
    add_subdirectory(mediapipe)
    set(ARM_LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS "mediapipe=${MEDIAPIPE_GIT_SHA}" PARENT_SCOPE)
    message(WARNING "Mediapipe support is currently experimental and limited to models like Gemma-2B, Phi-2 and Falcon-RW-1B")
elseif(${LLM_FRAMEWORK} STREQUAL "executorch")
    add_subdirectory(executorch)
    set(ARM_LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS "executorch=skeleton" PARENT_SCOPE)
    message(WARNING "ExecuTorch support is currently scaffold-only; inference entry points are not implemented yet")
else()
    message(FATAL_ERROR "${LLM_FRAMEWORK} is currently not supported :(")
endif()
