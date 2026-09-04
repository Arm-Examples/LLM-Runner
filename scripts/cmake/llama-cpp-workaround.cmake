#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# NOTE:
# This is a workaround for adding more backend variants for
# llama.cpp project. Because of the way CMake functions are
# defined to add new targets for different build variants, we
# copy the source tree so the relative paths from this
# directory scope resolve correctly.
set(LLAMA_WORKAROUND_SRC_DIR ${CMAKE_BINARY_DIR}/llama-additional-backends-src)
set(LLAMA_WORKAROUND_BIN_DIR ${CMAKE_BINARY_DIR}/llama-additional-backends-bin)
file(MAKE_DIRECTORY ${LLAMA_WORKAROUND_SRC_DIR})
file(COPY_FILE
    ${CMAKE_CURRENT_LIST_DIR}/llama-cpp-backends.cmake
    ${LLAMA_WORKAROUND_SRC_DIR}/CMakeLists.txt)
add_subdirectory(${LLAMA_WORKAROUND_SRC_DIR} ${LLAMA_WORKAROUND_BIN_DIR})
