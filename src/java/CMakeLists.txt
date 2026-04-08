#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(llm-java-prj
        VERSION     0.0.1
        DESCRIPTION "An LLM Java interface"
        LANGUAGES   C CXX ASM)

# Add the Java LLM API interface library. This is never going to be built
# but we can use its property to get the interface source used in other
# targets.

add_library(llm-java INTERFACE)
target_sources(llm-java INTERFACE com/arm/Llm.java)

add_dependencies(llm-java llm-jni)
