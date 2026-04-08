#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

project(llm-bench
        VERSION 0.0.1
        DESCRIPTION "LLM Benchmark"
        LANGUAGES CXX)

# Add the LLM chat library
add_library(llm-bench STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/LlmBench.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BenchRunner.cpp)

target_include_directories(llm-bench PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(llm-bench PUBLIC
    llm-framework
    llm-cpp
)

# Executable binary that uses the benchmark library
if(BUILD_BENCHMARK)
    add_executable(llm-bench-cli ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
    target_link_libraries(llm-bench-cli PRIVATE llm-bench)
endif()
