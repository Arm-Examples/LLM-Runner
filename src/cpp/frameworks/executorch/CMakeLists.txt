#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

project(executorch-wrapper
        DESCRIPTION "ExecuTorch wrapper interface implementation"
        LANGUAGES C CXX ASM)

include(FetchContent)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# --------------------------------------------------------------------
# ExecuTorch build
# --------------------------------------------------------------------
set(EXECUTORCH_SRC_DIR "${CMAKE_BINARY_DIR}/_deps/executorch"
        CACHE PATH "Path where ExecuTorch repo should be cloned")

set(EXECUTORCH_GIT_URL "https://github.com/pytorch/executorch.git"
        CACHE STRING "Git URL for ExecuTorch repo")

# Pinned release tag for deterministic runtime integration.
set(EXECUTORCH_GIT_TAG "v1.2.0"
        CACHE STRING "Git tag / commit SHA for ExecuTorch repo")

set(EXECUTORCH_BUILD_PRESET_FILE "${EXECUTORCH_SRC_DIR}/tools/cmake/preset/llm.cmake"
        CACHE FILEPATH "ExecuTorch preset file used to enable LLM runtime features")

# Text-path runtime support.
set(EXECUTORCH_BUILD_XNNPACK ON)
set(EXECUTORCH_BUILD_EXTENSION_DATA_LOADER ON)
set(EXECUTORCH_BUILD_EXTENSION_MODULE ON)
set(EXECUTORCH_BUILD_EXTENSION_TENSOR ON)
set(EXECUTORCH_BUILD_EXTENSION_LLM ON)
set(EXECUTORCH_BUILD_EXTENSION_LLM_RUNNER ON)
set(EXECUTORCH_BUILD_EXTENSION_RUNNER_UTIL ON)
# Enable the upstream LLM runtime kernels required by the current ExecuTorch
# text-model path, together with the optimized kernels they depend on.
set(EXECUTORCH_BUILD_KERNELS_LLM ON)
set(EXECUTORCH_BUILD_KERNELS_OPTIMIZED ON)
set(EXECUTORCH_BUILD_KERNELS_QUANTIZED ON)
set(EXECUTORCH_XNNPACK_ENABLE_WEIGHT_CACHE ON)
set(EXECUTORCH_XNNPACK_SHARED_WORKSPACE ON)
set(EXECUTORCH_XNNPACK_ENABLE_KLEIDI ${USE_KLEIDIAI})

# This integration only targets the native runtime and C++ runner libraries.
set(EXECUTORCH_BUILD_PYBIND OFF)

if (LLM_LOG_LEVEL STREQUAL "DEBUG")
    set(EXECUTORCH_MAPPED_LOG_LEVEL Debug)
elseif(LLM_LOG_LEVEL STREQUAL "INFO")
    set(EXECUTORCH_MAPPED_LOG_LEVEL Info)
else()
    # ExecuTorch does not expose a Warn level.
    set(EXECUTORCH_MAPPED_LOG_LEVEL Error)
endif()
set(EXECUTORCH_LOG_LEVEL ${EXECUTORCH_MAPPED_LOG_LEVEL}
        CACHE STRING "ExecuTorch log level mapped from LLM_LOG_LEVEL" FORCE)

execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import yaml, torchgen"
    RESULT_VARIABLE EXECUTORCH_PYTHON_DEPS_RESULT
    ERROR_VARIABLE EXECUTORCH_PYTHON_DEPS_ERROR
    OUTPUT_QUIET
    ERROR_STRIP_TRAILING_WHITESPACE)
if (NOT EXECUTORCH_PYTHON_DEPS_RESULT EQUAL 0)
    message(FATAL_ERROR
        "ExecuTorch configure requires the Python modules 'yaml' and 'torchgen'. "
        "Create a Python environment, install "
        "scripts/py/requirements-executorch.txt, and reconfigure with "
        "-DPython3_EXECUTABLE=<python-from-that-env>. "
        "Current interpreter: ${Python3_EXECUTABLE}. Python error: ${EXECUTORCH_PYTHON_DEPS_ERROR}")
endif()

# ExecuTorch's codegen modules expect both the repo root and src tree on
# PYTHONPATH during configure-time code generation.
if (DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
    set(ENV{PYTHONPATH} "${EXECUTORCH_SRC_DIR}/src:${EXECUTORCH_SRC_DIR}:$ENV{PYTHONPATH}")
else()
    set(ENV{PYTHONPATH} "${EXECUTORCH_SRC_DIR}/src:${EXECUTORCH_SRC_DIR}")
endif()

FetchContent_Declare(executorch
    GIT_REPOSITORY ${EXECUTORCH_GIT_URL}
    GIT_TAG        ${EXECUTORCH_GIT_TAG}
    GIT_SHALLOW    1
    GIT_SUBMODULES_RECURSE TRUE
    SOURCE_DIR     ${EXECUTORCH_SRC_DIR}
    EXCLUDE_FROM_ALL)

# Avoid clashing with the top-level nlohmann_json target that this project
# already defines for config parsing.
set(NLOHMANN_JSON_TARGET_NAME executorch_nlohmann_json)
FetchContent_MakeAvailable(executorch)
unset(NLOHMANN_JSON_TARGET_NAME)

set(EXECUTORCH_REQUIRED_TARGETS
    executorch
    executorch::backends
    executorch::extensions
    extension_llm_runner
    extension_runner_util
    extension_threadpool)

set(EXECUTORCH_RUNTIME_KERNEL_TARGETS
    quantized_ops_lib)

if (TARGET optimized_native_cpu_ops_lib)
    list(APPEND EXECUTORCH_RUNTIME_KERNEL_TARGETS optimized_native_cpu_ops_lib)
endif()

set(EXECUTORCH_RUNTIME_WHOLE_ARCHIVE_TARGETS)
if (TARGET custom_ops)
    list(APPEND EXECUTORCH_RUNTIME_WHOLE_ARCHIVE_TARGETS "$<LINK_LIBRARY:WHOLE_ARCHIVE,custom_ops>")
endif()

set(LLM_FRAMEWORK_REQUIRED_TARGETS
    llm-config
    llm-chat
    llm-interface
    llm-log)

set(REQUIRED_TARGETS
    ${EXECUTORCH_REQUIRED_TARGETS}
    ${EXECUTORCH_RUNTIME_KERNEL_TARGETS}
    ${LLM_FRAMEWORK_REQUIRED_TARGETS})

foreach(required_target IN LISTS REQUIRED_TARGETS)
    if (NOT TARGET ${required_target})
        message(FATAL_ERROR "${required_target} target not defined")
    endif()
endforeach()

# ExecuTorch exposes a helper that adds the platform-specific whole-archive
# linker flags required by kernel registration libraries. Apply it to every
# runtime kernel target we link so future registration libraries can be added
# through the same list safely.
foreach(executorch_runtime_kernel_target IN LISTS EXECUTORCH_RUNTIME_KERNEL_TARGETS)
    if (TARGET ${executorch_runtime_kernel_target})
        executorch_target_link_options_shared_lib(${executorch_runtime_kernel_target})
    endif()
endforeach()

add_library(llm-framework STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ExecuTorchImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmFactory.cpp)

target_include_directories(llm-framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llm-framework PUBLIC
    ${REQUIRED_TARGETS}
    ${EXECUTORCH_RUNTIME_WHOLE_ARCHIVE_TARGETS})
