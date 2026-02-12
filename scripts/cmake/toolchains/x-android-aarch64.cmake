#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

set(_host "${CMAKE_HOST_SYSTEM_NAME}")
string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" _proc)
set(TARGET_PLATFORM "android-aarch64")

# Host gating for the x-android-aarch64 .
# CMakePresets.json cannot conditionally enable/disable a preset based on host CPU
# (CMAKE_HOST_SYSTEM_PROCESSOR isn't available as a preset macro, and $HOSTTYPE isn't
# an environment variable), so we enforce host support here.
#
# Policy:
#   - macOS (Darwin): supported (including Apple Silicon / arm64)
#   - Linux: supported only on x86_64/amd64 hosts
#   - Linux aarch64/arm64: explicitly rejected
if(_host STREQUAL "Linux" AND _proc MATCHES "^(aarch64|arm64)$")
    message(FATAL_ERROR "x-android-aarch64 preset not supported on linux-aarch64, this option is supported on linux-x86_64 and macos.")
endif()


if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin"
    OR (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
        AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|i[3-6]86)$") )

  message(STATUS "Supported host: ${CMAKE_HOST_SYSTEM_NAME} / ${CMAKE_HOST_SYSTEM_PROCESSOR}")
else()
  message(FATAL_ERROR
    "Unsupported host: ${CMAKE_HOST_SYSTEM_NAME} / ${CMAKE_HOST_SYSTEM_PROCESSOR}. "
    "Cross compilation only supported on macOS and Linux x86_64.")
endif()


# Accept NDK_PATH from -DNDK_PATH=... or ENV{NDK_PATH}
if(NOT DEFINED NDK_PATH)
  if(DEFINED ENV{NDK_PATH})
    set(NDK_PATH "$ENV{NDK_PATH}" CACHE PATH "Path to Android NDK")
  else()
    message(FATAL_ERROR
      "NDK_PATH is not set. Pass -DNDK_PATH=/path/to/android-ndk "
      "or set the environment variable NDK_PATH.")
  endif()
endif()

# Normalize separators
file(TO_CMAKE_PATH "${NDK_PATH}" NDK_PATH)

set(ANDROID_TOOLCHAIN "${NDK_PATH}/build/cmake/android.toolchain.cmake")

if(NOT EXISTS "${ANDROID_TOOLCHAIN}")
  message(FATAL_ERROR
    "android.toolchain.cmake not found at: ${ANDROID_TOOLCHAIN}\n"
    "Check that NDK_PATH points to a valid Android NDK.")
endif()

message(STATUS "android-ndk.cmake: NDK='${ANDROID_NDK}' ABI='${ANDROID_ABI}' PLATFORM='${ANDROID_PLATFORM}'")


include("${ANDROID_TOOLCHAIN}")

set(CMAKE_CROSSCOMPILING ON)

include("${CMAKE_CURRENT_LIST_DIR}/base.cmake")

if(NOT DEFINED USE_KLEIDIAI OR USE_KLEIDIAI STREQUAL "")
   set(USE_KLEIDIAI ON)
endif()

message(STATUS "USE_KLEIDIAI=${USE_KLEIDIAI}")

# We default to share libs ON for llama.cpp on android.
if ("${LLM_FRAMEWORK}" STREQUAL "llama.cpp")
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" ON)
else()
    option(BUILD_SHARED_LIBS "BUILD_SHARED_LIBS" OFF)
endif()
message(STATUS "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}")

