#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Pull in LLM framework library
if (${LLM_DEP_NAME} STREQUAL "llama.cpp")
    add_subdirectory(llama_cpp)
else()
    message(FATAL_ERROR "${LLM_DEP_NAME} is currently not supported :(")
endif()
