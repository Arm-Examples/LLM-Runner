#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
cmake_minimum_required(VERSION 3.27)
project(mediapipe-cpp
        DESCRIPTION "Mediapipe wrapper interface implementation")

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(MEDIAPIPE_SRC_DIR "${CMAKE_BINARY_DIR}/mediapipe"
        CACHE PATH
        "Path where mediapipe repo should be cloned into")

set(MEDIAPIPE_GIT_URL "https://github.com/google-ai-edge/mediapipe.git"
        CACHE STRING
        "Git URL for mediapipe repo")

set(MEDIAPIPE_GIT_SHA "v0.10.26"
        CACHE STRING
        "Git commit SHA for mediapipe repo")

set(BASE_PATH "" CACHE STRING "Optional base-path of toolchain to build mediapipe for AARCH64")

set(MEDIAPIPE_BUILD_NUMBER 1)
# Clone the repository first
FetchContent_Declare(
        bazel_project
        GIT_REPOSITORY ${MEDIAPIPE_GIT_URL}
        GIT_TAG ${MEDIAPIPE_GIT_SHA}
        GIT_SHALLOW ${MEDIAPIPE_BUILD_NUMBER}
        SOURCE_DIR ${MEDIAPIPE_SRC_DIR}
)

FetchContent_MakeAvailable(bazel_project)

# Include the Bazel build process logic
include(bazel-build)

set(BAZEL_TARGET_DIR mediapipe/tasks/cc/genai/inference/c)

set(BAZEL_TARGET libllm_inference_engine_cpu)

set(LIB_NAME "${BAZEL_TARGET}.so")

set(MEDIAPIPE_LIB_INSTALL_PATH ${CMAKE_BINARY_DIR}/lib/${LIB_NAME})
set(MEDIAPIPE_INCLUDE_DIRECTORIES
        "${MEDIAPIPE_SRC_DIR}/${BAZEL_TARGET_DIR}"
        "${MEDIAPIPE_SRC_DIR}/mediapipe/framework/deps"
        "${MEDIAPIPE_SRC_DIR}/mediapipe/tasks/cc/genai/inference/c")

add_library(mediapipe-engine-cpu SHARED IMPORTED GLOBAL)

target_include_directories(mediapipe-engine-cpu INTERFACE ${MEDIAPIPE_INCLUDE_DIRECTORIES})

set_target_properties(mediapipe-engine-cpu PROPERTIES
        IMPORTED_LOCATION               "${MEDIAPIPE_LIB_INSTALL_PATH}"
        IMPORTED_NO_SONAME              ON
        INTERFACE_INCLUDE_DIRECTORIES   "${MEDIAPIPE_INCLUDE_DIRECTORIES}"
        INTERFACE_LINK_DIRECTORIES      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

# Make sure the mediapipe library is copied to the CMAKE_BINARY_DIR before linking
add_dependencies(mediapipe-engine-cpu bazel_build)

# Check internally defined dependency targets are visible
if (NOT TARGET arm-llm-config)
    message(FATAL_ERROR "arm-llm-config target not defined")
elseif (NOT TARGET arm-llm-interface)
    message(FATAL_ERROR "arm-llm-interface target not defined")
endif()

add_library(arm-llm-framework STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/MediaPipeImpl.cpp LlmFactory.cpp)

target_include_directories(arm-llm-framework PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

# List all libraries that we need to depend on here:
target_link_libraries(arm-llm-framework PUBLIC
        mediapipe-engine-cpu
        arm-llm-config
        arm-llm-interface)
