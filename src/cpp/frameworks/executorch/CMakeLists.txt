#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

project(executorch-wrapper
        DESCRIPTION "ExecuTorch wrapper interface implementation")

set(LLM_FRAMEWORK_REQUIRED_TARGETS
    llm-config
    llm-chat
    llm-interface
    llm-log)

foreach(required_target IN LISTS LLM_FRAMEWORK_REQUIRED_TARGETS)
    if (NOT TARGET ${required_target})
        message(FATAL_ERROR "${required_target} target not defined")
    endif()
endforeach()

add_library(llm-framework STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ExecuTorchImpl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmFactory.cpp)

target_include_directories(llm-framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llm-framework PUBLIC
    ${LLM_FRAMEWORK_REQUIRED_TARGETS})
