#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
project(llm-config
        VERSION 0.0.1
        DESCRIPTION "LLM configuration"
        LANGUAGES CXX)

# Add the LLM config library
add_library(llm-config STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmConfig.cpp)

target_include_directories(llm-config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

include(find_json)
target_link_libraries(llm-config PUBLIC nlohmann_json::nlohmann_json llm-log)

include(find_is_utf8)
target_link_libraries(llm-config PUBLIC is_utf8::is_utf8)
