#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)
include(configuration-options)
include(check-flag)
find_package(
        Python3 3.9...3.13
        COMPONENTS Interpreter
        REQUIRED)

if (NOT Python3_FOUND)
    message(FATAL_ERROR "Required version of Python3 not found!")
else()
    message(STATUS "Python3 (v${Python3_VERSION}) found: ${Python3_EXECUTABLE}")
endif()

set(OPTIONAL_FLAGS "")

if(ANDROID)
    message(STATUS "Building for Android")
    if(CMAKE_ANDROID_NDK)
        set(NDK_DIR ${CMAKE_ANDROID_NDK})
        set(OPTIONAL_FLAGS "--android-ndk-dir" ${NDK_DIR})
        list(APPEND OPTIONAL_FLAGS "--android-copy-path" ${CMAKE_SOURCE_DIR}/jniLibs/${CMAKE_ANDROID_ARCH_ABI})
        message(INFO "Added optional Android config parameters: ${OPTIONAL_FLAGS}")
    endif()
else()
    message(STATUS "Not building for Android")
endif()

# Finding Target architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    if(CMAKE_SYSTEM_NAME MATCHES "Android")
        set(TARGET_ARCH "ANDROID_ARM64")
    else()
        set(TARGET_ARCH "AARCH64")
        if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            list(APPEND OPTIONAL_FLAGS "--cross-compilation")
        endif()
        if (BASE_PATH)
           list(APPEND OPTIONAL_FLAGS "--base-path" ${BASE_PATH})
        endif()
    endif()
    set_kleidiai_flag()
    if(USE_KLEIDIAI)
        list(APPEND OPTIONAL_FLAGS "--use-kleidiai")
    endif()
    parse_arch_flag("i8mm")
    parse_arch_flag("dotprod")
    message("Target architecture is ${TARGET_ARCH}")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(TARGET_ARCH "X86")
    message("Target architecture is ${TARGET_ARCH}")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

execute_process(
        COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_LIST_DIR}/../py/build_mediapipe.py
        --mediapipe-dir
        ${MEDIAPIPE_SRC_DIR}
        --build-dir
        ${CMAKE_BINARY_DIR}/lib
        --target-arch
        ${TARGET_ARCH}
        ${OPTIONAL_FLAGS}
        RESULT_VARIABLE return_code)

if (NOT return_code STREQUAL "0")
    message(FATAL_ERROR "Failed to build mediapipe with bazel. Error code ${return_code}")
endif ()