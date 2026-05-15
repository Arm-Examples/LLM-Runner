#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
project(llm-framework-common
        VERSION 0.0.1
        DESCRIPTION "Common framework utilities"
        LANGUAGES CXX)

include(find_stb)

add_library(llm-framework-common STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageUtils.cpp)

target_include_directories(llm-framework-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llm-framework-common
    PUBLIC llm-log
    PRIVATE stb::stb)
