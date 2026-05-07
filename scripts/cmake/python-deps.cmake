#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
include_guard(DIRECTORY)
# -----------------------------------------------------------------------------
# Macro: llm_find_python_interpreter
#
# @brief Finds a supported Python3 interpreter (3.9...3.11).
#
# @return             None. Exits with a fatal error if Python3 is not found.
#
# Note: This is a macro (not a function) so find_package() variables remain
# visible to the caller scope.
# -----------------------------------------------------------------------------
macro(llm_find_python_interpreter)
    if (NOT (Python3_FOUND AND Python3_EXECUTABLE AND EXISTS "${Python3_EXECUTABLE}"))
        find_package(
                Python3 3.9...3.11
                COMPONENTS Interpreter
                REQUIRED)
        if (NOT Python3_FOUND)
            message(FATAL_ERROR "Required version of Python3 not found!")
        endif()
    endif()
    if (NOT DEFINED LLM_PYTHON_INTERPRETER_STATUS_PRINTED)
        message(STATUS "Python3 (supported: 3.9...3.11) found: v${Python3_VERSION} (${Python3_EXECUTABLE})")
        set(LLM_PYTHON_INTERPRETER_STATUS_PRINTED TRUE CACHE INTERNAL "Python3 interpreter status already printed")
    endif()
endmacro()
# -----------------------------------------------------------------------------
# Macro: llm_ensure_python_dependency
#
# @brief Ensures a Python module is importable; if not, installs its pip spec.
#
# @param MODULE_NAME  Python import name (e.g. "librosa", "huggingface_hub").
# @param PIP_SPEC     Pip install spec (e.g. "librosa==0.9.2", "huggingface_hub>=0.20.0").
#
# @return             None. Exits with a fatal error if dependency install fails.
# -----------------------------------------------------------------------------
macro(llm_ensure_python_dependency MODULE_NAME PIP_SPEC)
    llm_find_python_interpreter()
    execute_process(
            COMMAND ${Python3_EXECUTABLE} -c "import ${MODULE_NAME}"
            RESULT_VARIABLE llm_import_result
            OUTPUT_QUIET
            ERROR_QUIET)
    if (NOT llm_import_result EQUAL 0)
        message(STATUS "Installing Python dependency: ${PIP_SPEC}")
        execute_process(
                COMMAND ${Python3_EXECUTABLE} -m pip install "${PIP_SPEC}"
                RESULT_VARIABLE llm_pip_result)
        if (NOT llm_pip_result EQUAL 0)
            message(FATAL_ERROR "Failed to install Python dependency: ${PIP_SPEC}")
        endif()
    endif()
endmacro()
