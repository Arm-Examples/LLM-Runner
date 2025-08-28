#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
include_guard(DIRECTORY)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# -----------------------------------------------------------------------------
# Function: check_compiler_support
#
# @brief Checks if the compiler supports a specified flag for a given language (C,CXX).
#
# @param LANG   The programming language ("C" or "CXX") for which the flag is being tested.
# @param FLAG   The compiler flag to check for support.
#
# @return       None. Exits with a fatal error if the flag is unsupported.
# -----------------------------------------------------------------------------
function(check_compiler_support LANG FLAG)

    # Define the list of supported languages if it's not defined in CMakeLists.txt
    if (NOT SUPPORTED_LANGUAGES)
        set(SUPPORTED_LANGUAGES C CXX)
    endif()
    # Verify that the provided language is supported.
    if (NOT LANG IN_LIST SUPPORTED_LANGUAGES)
        message(FATAL_ERROR "Toolchain doesn't support this language flag '${LANG}'")
    endif()
    message(STATUS "Checking if compiler supports ${LANG} flag: ${FLAG}")
    # Construct the function name (e.g., check_c_compiler_flag) based on LANG.
    SET(FUNC_NAME "check_${LANG}_compiler_flag")
    # Invoke the corresponding flag-checking function and store the result in FLAG_SUPPORTED_<LANG>.
    cmake_language(CALL ${FUNC_NAME} "${FLAG}" FLAG_SUPPORTED_${LANG})
    if (NOT FLAG_SUPPORTED_${LANG})
        message(FATAL_ERROR "The compiler doesn't support the ${LANG} flag ${FLAG}!")
    else()
        message(STATUS "The compiler supports the ${LANG} flag ${FLAG}!")
    endif()
endfunction()

function(set_kleidiai_flag)

    # If the user has NOT explicitly set onnxruntime_USE_KLEIDIAI
    if (NOT DEFINED USE_KLEIDIAI)
        # if we are on arm64/aarch64, then default KleidiAI to ON.
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
            set(USE_KLEIDIAI ON CACHE BOOL
                "Enable KleidiAI by default on ${CMAKE_SYSTEM_PROCESSOR}")
            message(STATUS "KleidiAI enabled by default")
        # if we are NOT on arm64/aarch64, then default KleidiAI to OFF.
        else()
            set(USE_KLEIDIAI OFF CACHE BOOL
                "Disable KleidiAI by default on ${CMAKE_SYSTEM_PROCESSOR}")
            message(STATUS "KleidiAI disabled by default")
        endif()
    else ()
        message(STATUS "KleidiAI: ${USE_KLEIDIAI}")
    endif()
endfunction()

function(parse_arch_flag SUBSTRING)
    string(FIND "${CMAKE_C_FLAGS}" "${SUBSTRING}" _index)
    if(NOT _index EQUAL -1)
        set(updated_flags "${OPTIONAL_FLAGS}")
        list(APPEND updated_flags "--use-${SUBSTRING}")
        set(OPTIONAL_FLAGS "${updated_flags}" PARENT_SCOPE)
    endif()
endfunction()
