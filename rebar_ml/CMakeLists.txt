#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

option(
    USE_CF
    "Enable Compute Flow integration (bricks via a Compute Flow Application SDK)"
    OFF
)

# macOS builds: default policy floor for any nested CMake invocations that choose to
# consume it (kept consistent with Compute Flow superbuild conventions).
if(DEFINED TARGET_PLATFORM AND TARGET_PLATFORM STREQUAL "macos")
    if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM OR CMAKE_POLICY_VERSION_MINIMUM STREQUAL "")
        set(
            CMAKE_POLICY_VERSION_MINIMUM
            "3.5"
            CACHE STRING
            "Minimum CMake policy version used by nested builds (macOS default)"
        )
    endif()
endif()

if(USE_CF)
    add_subdirectory(cf)
endif()

add_subdirectory(core)

option(BUILD_REBAR_BENCHMARK "Build Rebar benchmarks" OFF)
if(BUILD_REBAR_BENCHMARK)
    add_subdirectory(benchmark)
endif()

if(BUILD_LLM_TESTING)
    add_subdirectory(test)
endif()