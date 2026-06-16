#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# We default to shared libs on for llama.cpp on Android.
# If the user attempts to configure this target with BUILD_SHARED_LIBS OFF
# a fatal error will be thrown in llama_cpp/CMakeLists.txt.
if (DEFINED ANDROID_ABI AND LLM_ENABLE_LLAMA_CPP)
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" ON)
else()
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" OFF)
endif()
message (STATUS "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

add_subdirectory(common)

if (NOT TARGET llm-config)
    message(FATAL_ERROR "llm-config target not defined")
elseif (NOT TARGET llm-chat)
    message(FATAL_ERROR "llm-chat target not defined")
elseif (NOT TARGET llm-interface)
    message(FATAL_ERROR "llm-interface target not defined")
endif()

if (NOT TARGET llm-framework-deps)
    add_library(llm-framework-deps INTERFACE)
    target_link_libraries(llm-framework-deps INTERFACE
        llm-config
        llm-chat
        llm-interface)
endif()

set(_arm_llm_backend_targets "")
set(_llm_backend_version_fragments "")
set(_llm_has_stb FALSE)

if (LLM_ENABLE_LLAMA_CPP)
    add_subdirectory(llama_cpp)
    list(APPEND _arm_llm_backend_targets arm-llm-framework-llama)
    list(APPEND _llm_backend_version_fragments "llama.cpp=${LLAMA_GIT_SHA}")
    list(APPEND _llm_backend_version_fragments "stb=${STB_GIT_TAG}")
    set(_llm_has_stb TRUE)
endif()

if (LLM_ENABLE_ONNXRUNTIME_GENAI)
    add_subdirectory(onnxruntime_genai)
    list(APPEND _arm_llm_backend_targets arm-llm-framework-onnxruntime-genai)
    list(APPEND _llm_backend_version_fragments
        "onnxruntime=${ONNXRUNTIME_GIT_TAG}"
        "onnxruntime-genai=${ONNXRT_GENAI_GIT_TAG}")
endif()

if (LLM_ENABLE_MNN)
    add_subdirectory(mnn)
    list(APPEND _arm_llm_backend_targets arm-llm-framework-mnn)
    list(APPEND _llm_backend_version_fragments "mnn=${MNN_GIT_TAG}")
    if (NOT _llm_has_stb)
        list(APPEND _llm_backend_version_fragments "stb=${STB_GIT_TAG}")
        set(_llm_has_stb TRUE)
    endif()
endif()

if (LLM_ENABLE_EXECUTORCH)
    add_subdirectory(executorch)
    list(APPEND _arm_llm_backend_targets arm-llm-framework-executorch)
    list(APPEND _llm_backend_version_fragments "executorch=${EXECUTORCH_GIT_TAG}")
endif()

list(LENGTH _arm_llm_backend_targets _llm_backend_targets_count)
if (_llm_backend_targets_count EQUAL 0)
    message(FATAL_ERROR "No backend targets were enabled. Check LLM_ENABLE_* options.")
endif()

string(JOIN ", " _llm_backend_versions ${_llm_backend_version_fragments})
set(LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS "${_llm_backend_versions}" PARENT_SCOPE)

if (NOT TARGET llm-framework)
    add_library(llm-framework STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/LlmFactory.cpp)

    target_include_directories(llm-framework PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..)

    target_link_libraries(llm-framework PUBLIC
        ${_arm_llm_backend_targets}
        llm-framework-deps
        llm-log)

    if (LLM_DEFAULT_BACKEND)
        target_compile_definitions(llm-framework PUBLIC LLM_DEFAULT_BACKEND="${LLM_DEFAULT_BACKEND}")
    endif()

    if (TARGET arm-llm-framework-llama)
        target_compile_definitions(llm-framework PUBLIC LLM_BACKEND_LLAMA_CPP=1)
    endif()
    if (TARGET arm-llm-framework-onnxruntime-genai)
        target_compile_definitions(llm-framework PUBLIC LLM_BACKEND_ONNXRUNTIME_GENAI=1)
    endif()
    if (TARGET arm-llm-framework-mnn)
        target_compile_definitions(llm-framework PUBLIC LLM_BACKEND_MNN=1)
    endif()
    if (TARGET arm-llm-framework-executorch)
        target_compile_definitions(llm-framework PUBLIC LLM_BACKEND_EXECUTORCH=1)
    endif()
    if (TARGET arm-llm-framework-llama AND TARGET arm-llm-framework-executorch)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_link_options(llm-framework PUBLIC "-Wl,--allow-multiple-definition")
        endif()
    endif()
endif()

target_link_libraries(llm-framework PUBLIC
    llm-framework-common
    llm-framework-deps
    llm-log)
