#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Pull in LLM framework library
if (${LLM_FRAMEWORK} STREQUAL "llama.cpp")
    add_subdirectory(llama_cpp)
elseif(${LLM_FRAMEWORK} STREQUAL "onnxruntime-genai")
    add_subdirectory(onnxruntime_genai)
elseif(${LLM_FRAMEWORK} STREQUAL "mnn")
    add_subdirectory(mnn)
elseif(${LLM_FRAMEWORK} STREQUAL "mediapipe")
    add_subdirectory(mediapipe)
    message(WARNING "Mediapipe support is currently experimental and limited to models like Gemma-2B, Phi-2 and Falcon-RW-1B")
else()
    message(FATAL_ERROR "${LLM_FRAMEWORK} is currently not supported :(")
endif()
