#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

if (NOT CMAKE_CROSSCOMPILING)
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    LLM_ENABLE_LLAMA_CPP
    CPU_ARCH
    TARGET_PLATFORM)
endif()

# BUILD_DEBUG defaults to OFF unless provided by CLI/presets.
if(NOT DEFINED BUILD_DEBUG OR NOT BUILD_DEBUG)
  set(BUILD_DEBUG OFF CACHE BOOL "Enable debug logging defaults")
  set (CMAKE_BUILD_TYPE "Release")
else()
  # Ensure it's shown as a BOOL in cmake-gui/ccmake, keep user's value.
  set_property(CACHE BUILD_DEBUG PROPERTY TYPE BOOL)
  set_property(CACHE BUILD_DEBUG PROPERTY HELPSTRING "Enable debug logging defaults")
  set (CMAKE_BUILD_TYPE "Debug")
endif()
message(STATUS "BUILD_DEBUG = ${BUILD_DEBUG}")

message(STATUS "BUILD_BENCHMARK   = ${BUILD_BENCHMARK}")
message(STATUS "BUILD_LLM_TESTING = ${BUILD_LLM_TESTING}")
message(STATUS "BUILD_JNI_LIB     = ${BUILD_JNI_LIB}")
message(STATUS "CMAKE_BUILD_TYPE  = ${CMAKE_BUILD_TYPE}")

# Only set (Global) LOG_LEVEL if not provided by user (CLI/presets)
if(NOT DEFINED LOG_LEVEL OR LOG_LEVEL STREQUAL "")

  if(BUILD_DEBUG)             # TRUE if ON/TRUE/1/YES
    set(LOG_LEVEL "DEBUG" CACHE STRING "Global Logging level")
  else()
    set(LOG_LEVEL "INFO"  CACHE STRING "Global Logging level")
  endif()

  # Optional: show allowed values
  set_property(CACHE LOG_LEVEL PROPERTY STRINGS TRACE DEBUG INFO WARN ERROR)
endif()
message(STATUS "Global LOG_LEVEL = ${LOG_LEVEL}")


# Only use the global log level to set LLM_LOG_LEVEL if it has not been
# provided by user
if(NOT DEFINED LLM_LOG_LEVEL OR LLM_LOG_LEVEL STREQUAL "")
  set(LLM_LOG_LEVEL ${LOG_LEVEL} CACHE STRING "LLM Logging level")
endif()
message(STATUS "LLM_LOG_LEVEL = ${LLM_LOG_LEVEL}")
