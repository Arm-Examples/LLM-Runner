#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
project(llm-chat
        VERSION 0.0.1
        DESCRIPTION "LLM chat"
        LANGUAGES CXX)

# Add the LLM chat library
add_library(llm-chat STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmChat.cpp)

target_include_directories(llm-chat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llm-chat PUBLIC llm-framework)