#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
# Usage: this file should be copied over as a CMakeLists.txt file
# in the binary tree and added as a subdirectory from there.

if (NOT DEFINED LLAMA_SRC_DIR)
    message(FATAL_ERROR "LLAMA_SRC_DIR should be defined.")
endif()

if (GGML_CPU_ALL_VARIANTS)
    if (ANDROID_ABI STREQUAL "arm64-v8a")
        # Copy the ggml-cpu directory from llama.cpp source tree in the
        # same location as this file.
        file(COPY ${LLAMA_SRC_DIR}/ggml/src/ggml-cpu DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

        file(GLOB GGML_PRIVATE_HEADERS "${LLAMA_SRC_DIR}/ggml/src/*.h")
        file(COPY ${GGML_PRIVATE_HEADERS} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})

        file(GLOB GGML_PUBLIC_HEADERS "${LLAMA_SRC_DIR}/ggml/include/*.h")
        file(COPY ${GGML_PRIVATE_HEADERS} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
        file(COPY ${GGML_PUBLIC_HEADERS} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
        set(GGML_SYSTEM_ARCH "ARM")

        # Change target lib location for all
        list(APPEND TARGET_LIBS_FOR_ANDROID
            llama
            mtmd
            ggml-base
            ggml
            ggml-cpu-android_armv8.0_1
            ggml-cpu-android_armv8.2_1
            ggml-cpu-android_armv8.2_2
            ggml-cpu-android_armv8.6_1
            ggml-cpu-android_armv9.0_1
            ggml-cpu-android_armv9.2_1
            ggml-cpu-android_armv9.2_2
            )

        foreach(TAR ${TARGET_LIBS_FOR_ANDROID})
            if(TARGET ${TAR})
                message(STATUS "Overriding lib output location for ${TAR}")
                set_target_properties(${TAR} PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
            endif()
        endforeach()

    else()
        if (TARGET ggml-cpu-armv8.0_1)
            target_compile_options(ggml-cpu-armv8.0_1 PRIVATE -Wno-format-zero-length -Wno-pedantic)
        endif()
    endif()
endif()
