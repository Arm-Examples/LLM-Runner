#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(llm-interface
        DESCRIPTION "An LLM CPP interface")

# Add the LLM API interface library:
add_library(llm-interface INTERFACE)
target_include_directories(llm-interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR})
