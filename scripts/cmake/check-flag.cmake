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
