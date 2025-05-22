#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
project(arm-llm-config
        VERSION 0.0.1
        DESCRIPTION "LLM configuration"
        LANGUAGES CXX)

# Add the LLM config library
add_library(arm-llm-config STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmConfig.cpp)

target_include_directories(arm-llm-config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
