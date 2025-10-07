#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
include_guard(DIRECTORY)

include(FetchContent)

FetchContent_Declare(is_utf8
    GIT_REPOSITORY https://github.com/simdutf/is_utf8.git
    GIT_TAG v1.4.1
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(is_utf8)