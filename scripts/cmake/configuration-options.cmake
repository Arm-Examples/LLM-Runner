#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

set(DOWNLOADS_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/resources_downloaded
    CACHE STRING
    "Directory where required resources are downloaded into")

set(DOWNLOADS_LOCK_TIMEOUT 600
    CACHE STRING
    "Timeout in seconds for lock to hold off concurrent CMake configurations
    trying to download resources to the same directory.")

option(BUILD_BENCHMARK     "Build benchmark binary"               ON)
option(BUILD_LLM_TESTING   "Build unit tests"                     ON)
option(BUILD_JNI_LIB       "Build JNI lib"                        ON)
option(LLM_JNI_TIMING      "Enable JNI timing helpers"            OFF)
option(LLAMA_BUILD_COMMON  "Include LLAMA common"                 ON)
option(ENABLE_STREAMLINE   "Enable Arm Streamline annotations"    OFF)
option(DOWNLOAD_LLM_MODELS "Download LLM models during configure" ON)

set(_llm_llama_cpp_user_set OFF)
if(DEFINED CACHE{LLM_ENABLE_LLAMA_CPP})
    set(_llm_llama_cpp_user_set ON)
endif()

set(_llm_other_backend_enabled OFF)
foreach(_llm_backend_var IN ITEMS LLM_ENABLE_ONNXRUNTIME_GENAI LLM_ENABLE_MNN LLM_ENABLE_EXECUTORCH)
    if(DEFINED CACHE{${_llm_backend_var}})
        get_property(_llm_backend_value CACHE ${_llm_backend_var} PROPERTY VALUE)
        if(_llm_backend_value)
            set(_llm_other_backend_enabled ON)
        endif()
    endif()
endforeach()

set(_llm_llama_cpp_default ON)
if(NOT _llm_llama_cpp_user_set AND _llm_other_backend_enabled)
    set(_llm_llama_cpp_default OFF)
endif()

option(LLM_ENABLE_LLAMA_CPP         "Enable the llama.cpp backend"            ${_llm_llama_cpp_default})
option(LLM_ENABLE_ONNXRUNTIME_GENAI "Enable the onnxruntime-genai backend"    OFF)
option(LLM_ENABLE_MNN               "Enable the MNN backend"                  OFF)
option(LLM_ENABLE_EXECUTORCH        "Enable the ExecuTorch backend"           OFF)
set(LLM_DEFAULT_FRAMEWORK "" CACHE STRING
    "Default framework to use when multiple backends are enabled")

set(_llm_enabled_backends "")
if (LLM_ENABLE_LLAMA_CPP)
    list(APPEND _llm_enabled_backends "llama.cpp")
endif()
if (LLM_ENABLE_ONNXRUNTIME_GENAI)
    list(APPEND _llm_enabled_backends "onnxruntime-genai")
endif()
if (LLM_ENABLE_MNN)
    list(APPEND _llm_enabled_backends "mnn")
endif()
if (LLM_ENABLE_EXECUTORCH)
    list(APPEND _llm_enabled_backends "executorch")
endif()

list(LENGTH _llm_enabled_backends _llm_enabled_backends_count)
if (_llm_enabled_backends_count EQUAL 0)
    message(FATAL_ERROR "No LLM backends are enabled. Set LLM_ENABLE_* options.")
endif()

set(_llm_ordered_backends "")
set(_llm_preferred_backends "llama.cpp" "mnn" "onnxruntime-genai" "executorch")
foreach(_backend IN LISTS _llm_preferred_backends)
    list(FIND _llm_enabled_backends "${_backend}" _llm_preferred_index)
    if (NOT _llm_preferred_index EQUAL -1)
        list(APPEND _llm_ordered_backends "${_backend}")
    endif()
endforeach()
foreach(_backend IN LISTS _llm_enabled_backends)
    list(FIND _llm_ordered_backends "${_backend}" _llm_present_index)
    if (_llm_present_index EQUAL -1)
        list(APPEND _llm_ordered_backends "${_backend}")
    endif()
endforeach()

set(LLM_ENABLED_BACKENDS "${_llm_ordered_backends}" CACHE STRING
    "List of enabled LLM backends" FORCE)

if (_llm_enabled_backends_count GREATER 1)
    set(LLM_MULTI_BACKEND ON)
else()
    set(LLM_MULTI_BACKEND OFF)
endif()

set(_llm_default_backend "")
if (_llm_enabled_backends_count EQUAL 1)
    list(GET _llm_enabled_backends 0 _llm_default_backend)
elseif (LLM_DEFAULT_FRAMEWORK)
    list(FIND _llm_enabled_backends "${LLM_DEFAULT_FRAMEWORK}" _llm_default_index)
    if (_llm_default_index EQUAL -1)
        message(FATAL_ERROR
            "LLM_DEFAULT_FRAMEWORK='${LLM_DEFAULT_FRAMEWORK}' is not enabled. "
            "Enabled backends: ${_llm_enabled_backends}")
    endif()
    set(_llm_default_backend "${LLM_DEFAULT_FRAMEWORK}")
else()
    list(GET _llm_ordered_backends 0 _llm_default_backend)
endif()

set(LLM_DEFAULT_BACKEND "${_llm_default_backend}")
