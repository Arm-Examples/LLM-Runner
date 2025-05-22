#
# SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
add_subdirectory(cpp)

if(BUILD_JNI_LIB)
    add_subdirectory(java)
endif()
