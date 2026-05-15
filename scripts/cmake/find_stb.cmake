#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
include_guard(DIRECTORY)

include(FetchContent)

set(STB_GIT_URL "https://github.com/nothings/stb.git"
    CACHE STRING "Git URL for stb headers")

set(STB_GIT_TAG "31c1ad37456438565541f4919958214b6e762fb4"
    CACHE STRING "Git tag / commit SHA for stb headers")

FetchContent_Declare(stb
    GIT_REPOSITORY ${STB_GIT_URL}
    GIT_TAG        ${STB_GIT_TAG}
    GIT_SHALLOW    1
    EXCLUDE_FROM_ALL
)

FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

if(NOT TARGET stb::stb)
    add_library(stb INTERFACE)
    add_library(stb::stb ALIAS stb)
    target_include_directories(stb INTERFACE "${stb_SOURCE_DIR}")
endif()
