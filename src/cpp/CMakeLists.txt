#
# SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(arm-llm-cpp-prj
        DESCRIPTION "An LLM CPP library"
        LANGUAGES   C CXX ASM)

add_subdirectory(log)

# Add interface:
add_subdirectory(interface)

# Add configuration lib:
add_subdirectory(config)

# Add chat lib;
add_subdirectory(chat)

# Add frameworks lib:
add_subdirectory(frameworks)

# Actual API library that depends on framework
add_library(arm-llm-cpp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Llm.cpp)

# Libraries needed
target_link_libraries(arm-llm-cpp PUBLIC
    arm-llm-interface
    arm-llm-config
    arm-llm-framework
    arm-llm-chat
    arm-llm-log)

# If building with JNI support, build a separate
# target to provide these bindings.
if (BUILD_JNI_LIB)
    # Make sure JNI include directories have been set.
    include(find-jni)

    add_library(arm-llm-jni SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/LlmJni.cpp)

    target_link_libraries(arm-llm-jni PUBLIC arm-llm-cpp)

    target_include_directories(arm-llm-jni PUBLIC
        ${JNI_INCLUDE_DIRS} # Populated by FindJNI CMake module
    )
endif()
