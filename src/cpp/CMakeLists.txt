#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(arm-llm-cpp-prj
        DESCRIPTION "An LLM CPP library"
        LANGUAGES   C CXX ASM)

# Add log
add_subdirectory(log)

# Add interface:
add_subdirectory(interface)

# Add configuration lib:
add_subdirectory(config)

# Add chat lib;
add_subdirectory(chat)

set(ARM_LLM_BUILD_FRAMEWORK_NAME "${LLM_FRAMEWORK}")
set(ARM_LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS "unknown")

# Add frameworks lib:
add_subdirectory(frameworks)

set(ARM_LLM_BUILD_INFO_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${ARM_LLM_BUILD_INFO_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/log/LlmBuildInfoConfig.hpp.in"
    "${ARM_LLM_BUILD_INFO_DIR}/LlmBuildInfoConfig.hpp"
    @ONLY)
target_include_directories(arm-llm-log PUBLIC "${ARM_LLM_BUILD_INFO_DIR}")

# Add benchmark lib:
add_subdirectory(benchmark)

# Actual API library that depends on framework
add_library(arm-llm-cpp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Llm.cpp ${CMAKE_CURRENT_SOURCE_DIR}/LlmBridge.cpp)

# Libraries needed
target_link_libraries(arm-llm-cpp PUBLIC
    arm-llm-interface
    arm-llm-config
    arm-llm-framework
    arm-llm-chat
    arm-llm-log)

# Enable streamline annotation
if(ENABLE_STREAMLINE)
    include(streamline)
    arm_enable_streamline(arm-llm-cpp)
endif()

# If building with JNI support, build a separate
# target to provide these bindings.
if (BUILD_JNI_LIB)
    # Make sure JNI include directories have been set.
    include(find-jni)

    add_library(arm-llm-jni SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/LlmJni.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/LlmCache.cpp)

    target_link_libraries(arm-llm-jni PUBLIC 
        arm-llm-cpp 
        arm-llm-bench)

    target_include_directories(arm-llm-jni PUBLIC
        ${JNI_INCLUDE_DIRS} # Populated by FindJNI CMake module
    )
endif()
