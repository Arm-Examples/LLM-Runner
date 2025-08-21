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

message(STATUS "Using CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")

# Declare project
project(arm-llm-wrapper
        VERSION     0.0.1
        DESCRIPTION "An LLM interface library"
        LANGUAGES   C CXX ASM)

if (USE_KLEIDIAI)
    # Ensure target or host is Arm
    if (NOT CMAKE_CROSSCOMPILING)
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROC_LOWER)
        if (NOT SYSTEM_PROC_LOWER MATCHES "^(aarch64|arm64)$")
            message(WARNING
                    "KleidiAI=ON but not on an Arm host (${CMAKE_SYSTEM_PROCESSOR}).")
        endif()
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake")

# Set the runtime and lib directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/archive)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Use Release as the default build type if none was specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Set position independent code enabled
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Make coverage option available and wire flags if enabled
include(enable-coverage)

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
