#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(DIRECTORY)

Include(FetchContent)

if (NOT DEFINED CATCH_DIR)
    set(CATCH_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY  https://github.com/catchorg/Catch2.git
    GIT_TAG         v3.10.0
    GIT_SHALLOW     1
    EXCLUDE_FROM_ALL
)

if (NOT DEFINED CATCH_DIR)
    set(CATCH_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

FetchContent_MakeAvailable(Catch2)
