#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

set(_host "${CMAKE_HOST_SYSTEM_NAME}")
string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" _proc)

# Can't put this check / filter in CMakePresets.json, CMAKE_HOST_SYSTEM_PROCESSOR isn't an available macro 
# in cmake presets. Bash shell's $HOSTTYPE is an shell variable not an environmental variable therefore isn't
# available either. Best effort is to add the check here!
if(_host STREQUAL "Linux" AND _proc MATCHES "^(aarch64|arm64)$")
    message(FATAL_ERROR "x-linux-aarch64 preset not supported on linux-aarch64 use 'cmake --preset=native' instead.")
endif()


set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CROSSCOMPILING true)
set(CMAKE_SYSTEM_NAME Linux)
set(TARGET_PLATFORM "linux-aarch64")

set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CPU_ARCH TARGET_PLATFORM LLM_ENABLE_LLAMA_CPP)

include("${CMAKE_CURRENT_LIST_DIR}/base.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/aarch64-base.cmake")

set(GNU_MACHINE "aarch64-none-linux-gnu-")
set(CROSS_PREFIX "aarch64-none-linux-gnu-")

set(CMAKE_C_COMPILER   ${CROSS_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_PREFIX}g++)
set(CMAKE_AR           ${CROSS_PREFIX}ar)
set(CMAKE_STRIP        ${CROSS_PREFIX}strip)
set(CMAKE_LINKER       ${CROSS_PREFIX}ld)
