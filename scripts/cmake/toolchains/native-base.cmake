#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/base.cmake")


string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" _build_cpu_lc)
set(_is_x86_64 FALSE)
if(_build_cpu_lc MATCHES "^(x86_64|amd64)$") # treat amd64 as x86_64
  set(_is_x86_64 TRUE)
endif()

message(STATUS "USE_KLEIDIAI = ${USE_KLEIDIAI} (build CPU: ${CMAKE_HOST_SYSTEM_PROCESSOR})")

if(DEFINED CACHE{USE_KLEIDIAI})
  # keep user's value; make it a BOOL in GUIs
  set_property(CACHE USE_KLEIDIAI PROPERTY TYPE BOOL)
  set_property(CACHE USE_KLEIDIAI PROPERTY HELPSTRING "Enable Arm KleidiAI kernels")

  # Warn if ON on x86_64 build CPU
  if(USE_KLEIDIAI AND _is_x86_64)
    message(WARNING
      "USE_KLEIDIAI=ON with build CPU ${CMAKE_HOST_SYSTEM_PROCESSOR} (x86_64). "
      "KleidiAI targets Arm; this may have no effect or cause issues.")
  endif()

else()
  # Not set by user: default OFF on x86_64, ON otherwise
  if(_is_x86_64)
    set(USE_KLEIDIAI OFF CACHE BOOL "Enable Arm KleidiAI kernels (default OFF on x86_64)")
  else()
    set(USE_KLEIDIAI ON  CACHE BOOL "Enable Arm KleidiAI kernels (default ON on non-x86_64)")
  endif()
endif()

message(STATUS "USE_KLEIDIAI = ${USE_KLEIDIAI} (build CPU: ${CMAKE_HOST_SYSTEM_PROCESSOR})")


# CPU_ARCH may be specified only when:
# llama.cpp is enabled AND target == Linux/aarch64
if(DEFINED CPU_ARCH AND
   NOT ((DEFINED LLM_ENABLE_LLAMA_CPP AND LLM_ENABLE_LLAMA_CPP)
        AND "${TARGET_PLATFORM}"  STREQUAL "linux-aarch64"))
  
  message(FATAL_ERROR
    "CPU_ARCH is set ('${CPU_ARCH}'), but this is only allowed when "
    "llama.cpp is enabled AND target platform is Linux/aarch64. "
    "Current target=${TARGET_PLATFORM}.")
endif()
