#
# SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.27)

set(CMAKE_CXX_STANDARD 17)

# If CMake toolchain is not defined, set it here.
if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/toolchains/native.cmake")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "Using CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

# Declare project
project(arm-llm-wrapper
        VERSION     0.0.1
        DESCRIPTION "An LLM interface library"
        LANGUAGES   C CXX ASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake")

# Set the runtime and lib directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set position independent code enabled
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add default configuration options users can set from command line.
include(configuration-options)

# Download the common resources that may be used. Once these are set up
# in the repository, they won't be downloaded again for each build.
include(download-resources)

# Add sources or interface files to the build.
add_subdirectory(src)

# Add tests if they are enabled.
if(BUILD_UNIT_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
