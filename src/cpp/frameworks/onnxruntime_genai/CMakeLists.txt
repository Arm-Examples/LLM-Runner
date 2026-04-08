#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(onnxruntime-genai-wrapper
        DESCRIPTION "ONNX Runtime GenAI wrapper interface implementation"
        LANGUAGES C CXX ASM)

include(FetchContent)
include(check-flag)

# Disable GCC 14's range-loop-construct warning-as-error
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 14)
  message(STATUS "GCC >= 14 detected, disabling -Werror for range-loop-construct")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=range-loop-construct")
endif()

# Disabled array-bounds warning-as-error since NDK clang triggers it in MLAS code.
# Setting it at this project level to ensure we cover all onnx binaries
# ONNX project probably don't compile with these flags because if they did they 
# probably would have seen this problem
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=array-bounds")

# --------------------------------------------------------------------
# onnxruntime build
# --------------------------------------------------------------------

# KleidiAI configuration
if(DEFINED onnxruntime_USE_KLEIDIAI)
    message(FATAL_ERROR "Don't use framework specific KleidiAI flag, configure 'USE_KLEIDIAI' flag instead.")
else()
    set_kleidiai_flag()
    set(onnxruntime_USE_KLEIDIAI  ${USE_KLEIDIAI})
endif()

# --------------------------------------------------------------------
# onnxruntime build flags
# onnxruntime_BUILD_SHARED_LIB needs to be ON to produce libonnxruntime.so
# which is a dependency for libonnruntime-genai.so
# --------------------------------------------------------------------

set(onnxruntime_BUILD_UNIT_TESTS                  OFF)
set(onnxruntime_ENABLE_TESTS                      OFF)
set(onnxruntime_DISABLE_RTTI                      ON)
set(onnxruntime_DISABLE_FLOAT8_TYPES              ON)
set(onnxruntime_ENABLE_EXTERNAL_CUSTOM_OP_SCHEMAS OFF)
set(onnxruntime_USE_MIMALLOC                      OFF)
set(onnxruntime_BUILD_SHARED_LIB                  ON)
set(ONNX_BUILD_SHARED_LIBS                        OFF)

set(ONNXRUNTIME_GIT_URL   "https://github.com/microsoft/onnxruntime.git"
        CACHE STRING "Git URL for onnxruntime repo")
        
# Latest stable tag (March 2026)
set(ONNXRUNTIME_GIT_TAG   "v1.24.2"
        CACHE STRING "Path where onnxruntime repo should be cloned")

set(ONNXRUNTIME_SRC_DIR   "${CMAKE_BINARY_DIR}/onnxruntime"
        CACHE PATH   "Source dir")

set(ONNXRUNTIME_BUILD_DIR "${CMAKE_BINARY_DIR}/ort_build"
        CACHE PATH   "Onnxruntime build dir")

file(MAKE_DIRECTORY "${ONNXRUNTIME_BUILD_DIR}")

FetchContent_Declare(
  onnxruntime
    GIT_REPOSITORY ${ONNXRUNTIME_GIT_URL}
    GIT_TAG        ${ONNXRUNTIME_GIT_TAG}
    SOURCE_DIR     ${ONNXRUNTIME_SRC_DIR}
    GIT_SHALLOW    1
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(onnxruntime)

add_subdirectory(
  ${ONNXRUNTIME_SRC_DIR}/cmake
  ${ONNXRUNTIME_BUILD_DIR})

# ---------------------------------------------------------------------------
# Temporary workaround:
#
# The onnxruntime-genai build expects the onnxruntime header files and the shared
# library to reside in a very specific folder structure at configuration
# time.
#
# To build onnxruntime from source (avoiding the pre-built download), we create a
# temporary staging directory that:
#   - Copies in the onnxruntime headers required by onnxruntime-genai, and
#   - Drops in a placeholder onnxruntime shared library just long enough for
#     onnxruntime-genai to detect it (the file is removed immediately afterward).
#
# The actual onnxruntime shared library produced by the build is written to
# ${CMAKE_BINARY_DIR}/lib.
# ---------------------------------------------------------------------------

set(ORT_HOME "${ONNXRUNTIME_BUILD_DIR}/ort-tmp-src" CACHE STRING "Ort path")

if(ANDROID)
    set(ORT_INCLUDE_DIR "${ORT_HOME}/headers")
    set(ORT_LIB_DIR     "${ORT_HOME}/jni/${ANDROID_ABI}")
else()
    set(ORT_INCLUDE_DIR "${ORT_HOME}/include")
    set(ORT_LIB_DIR     "${ORT_HOME}/lib")
endif()

file(MAKE_DIRECTORY "${ORT_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${ORT_LIB_DIR}")

set(ORT_HEADERS_DIR "${ONNXRUNTIME_SRC_DIR}/include/onnxruntime/core/session/")

file(COPY ${ORT_HEADERS_DIR} DESTINATION ${ORT_INCLUDE_DIR})
file(TOUCH ${ORT_LIB_DIR}/libonnxruntime.so "")
file(TOUCH ${ORT_LIB_DIR}/libonnxruntime.dylib "")

# --------------------------------------------------------------------
# onnxruntime-genai build
# --------------------------------------------------------------------

set(ONNXRT_GENAI_SRC_DIR "${CMAKE_BINARY_DIR}/onnxruntime-genai"
        CACHE PATH  "Path where onnxruntime-genai repo should be cloned")

set(ONNXRT_GENAI_GIT_URL "https://github.com/microsoft/onnxruntime-genai.git"
        CACHE STRING "Git URL for onnxruntime-genai repo")

# Latest stable tag (March 2026)
set(ONNXRT_GENAI_GIT_TAG "v0.12.0"
        CACHE STRING "Git tag / commit SHA for onnxruntime-genai repo")

# --------------------------------------------------------------------
# onnxruntime-genai build flags
# Disable GPU execution providers unless the caller turns them back on
# (e.g. cmake .. -DUSE_CUDA=ON).
# --------------------------------------------------------------------

set(USE_CUDA               OFF)
set(USE_ROCM               OFF)
set(USE_DML                OFF)
set(ENABLE_PYTHON          OFF)
set(ENABLE_JAVA            OFF)
set(ENABLE_TESTS           OFF)

# model-benchmark standalone application
set(ENABLE_MODEL_BENCHMARK ${BUILD_BENCHMARK} CACHE BOOL  "Enable model benchmark binary")

# Fetch the dependency Git repo here
FetchContent_Declare(onnxruntime-genai
    GIT_REPOSITORY ${ONNXRT_GENAI_GIT_URL}
    GIT_TAG ${ONNXRT_GENAI_GIT_TAG}
    GIT_SHALLOW 1
    SOURCE_DIR ${ONNXRT_GENAI_SRC_DIR})

FetchContent_MakeAvailable(onnxruntime-genai)

# Remove the created onnxruntime shared library
file(REMOVE_RECURSE ${ORT_LIB_DIR})

# Check internally defined dependency targets are visible
if (NOT TARGET llm-config)
    message(FATAL_ERROR "llm-config target not defined")
elseif (NOT TARGET llm-chat)
    message(FATAL_ERROR "llm-chat target not defined")
elseif (NOT TARGET llm-interface)
    message(FATAL_ERROR "llm-interface target not defined")
elseif (NOT TARGET llm-log)
    message(FATAL_ERROR "llm-log target not defined")
endif()

add_library(llm-framework STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/OnnxrtImpl.cpp LlmFactory.cpp)

target_include_directories(llm-framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ONNXRT_GENAI_SRC_DIR}/src)
# List all libraries that we need to depend on here:
target_link_libraries(llm-framework PUBLIC
    onnxruntime-genai
    onnxruntime
    llm-config
    llm-chat
    llm-interface
    llm-log)

if(BUILD_BENCHMARK)
    target_include_directories(model_benchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ONNXRT_GENAI_SRC_DIR}/src)

    target_link_libraries(model_benchmark PRIVATE onnxruntime-genai ${ONNXRUNTIME_LIB})
    target_link_directories(model_benchmark PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    add_dependencies(model_benchmark onnxruntime)
endif()
