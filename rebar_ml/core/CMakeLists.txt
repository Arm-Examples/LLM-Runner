#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

add_subdirectory(src/cpp)
if(BUILD_JNI_LIB AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/java/CMakeLists.txt")
    add_subdirectory(src/java)
endif()
