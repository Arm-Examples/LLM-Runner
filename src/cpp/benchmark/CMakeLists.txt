#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

project(arm-llm-bench
        VERSION 0.0.1
        DESCRIPTION "LLM Benchmark"
        LANGUAGES CXX)

# Add the LLM chat library
add_library(arm-llm-bench STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmBenchmark.cpp)

target_include_directories(arm-llm-bench PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(arm-llm-bench PUBLIC
    arm-llm-framework
    arm-llm-cpp
)

# Executable binary that uses the benchmark library
if(BUILD_BENCHMARK)
    add_executable(arm-llm-bench-cli ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
    target_link_libraries(arm-llm-bench-cli PRIVATE arm-llm-bench)
endif()
