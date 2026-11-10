#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

set(LLM_ORIGIN_RPATH "")
if(DEFINED TARGET_PLATFORM AND TARGET_PLATFORM STREQUAL "macos")
    set(LLM_ORIGIN_RPATH "@loader_path")
elseif(UNIX AND NOT (ANDROID OR DEFINED ANDROID_ABI))
    set(LLM_ORIGIN_RPATH "$ORIGIN")
endif()

function(llm_configure_rpath target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "llm_configure_rpath: target does not exist: ${target_name}")
    endif()

    get_target_property(_target_type "${target_name}" TYPE)

    if(LLM_ORIGIN_RPATH AND (_target_type STREQUAL "SHARED_LIBRARY" OR _target_type STREQUAL "MODULE_LIBRARY"))
        set_target_properties("${target_name}" PROPERTIES BUILD_RPATH "${LLM_ORIGIN_RPATH}")
    endif()

    # On ELF platforms, tell CMake to prefer $ORIGIN-style build-tree rpaths.
    # macOS uses @loader_path and does not support BUILD_RPATH_USE_ORIGIN.
    if(UNIX AND NOT (ANDROID OR DEFINED ANDROID_ABI) AND LLM_ORIGIN_RPATH STREQUAL "$ORIGIN")
        set_target_properties("${target_name}" PROPERTIES BUILD_RPATH_USE_ORIGIN TRUE)
    endif()
endfunction()
