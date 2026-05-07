#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)
include(configuration-options)

include(python-deps)

set(LLM_HF_HUB_PIP_PACKAGE "huggingface_hub")
set(LLM_HF_HUB_PIP_CONSTRAINT ">=0.20.0")
set(LLM_HF_HUB_PIP_SPEC "${LLM_HF_HUB_PIP_PACKAGE}${LLM_HF_HUB_PIP_CONSTRAINT}")
llm_ensure_python_dependency("huggingface_hub" "${LLM_HF_HUB_PIP_SPEC}")

# If the downloads directory doesn't exist, create one
if (NOT EXISTS ${DOWNLOADS_DIR})
    file(MAKE_DIRECTORY ${DOWNLOADS_DIR})
endif()

# Create a lock so other instances of CMake configuration processes hold
# here until the lock is available.
message(STATUS "Waiting to lock resource ${DOWNLOADS_DIR} "
               "Timeout: ${DOWNLOADS_LOCK_TIMEOUT} seconds.")
file(LOCK ${DOWNLOADS_DIR}
    DIRECTORY
    GUARD PROCESS
    RESULT_VARIABLE lock_return_code
    TIMEOUT ${DOWNLOADS_LOCK_TIMEOUT})

if (NOT ${lock_return_code} STREQUAL 0)
    message(FATAL_ERROR "Failed to acquire lock for dir ${DOWNLOADS_DIR}")
endif()
message(STATUS "${DOWNLOADS_DIR} locked; running downloads script...")

if (DOWNLOAD_LLM_MODELS)
    # Hugging Face token from command line argument takes precedence
    if(DEFINED HF_TOKEN AND NOT HF_TOKEN STREQUAL "")
        set(HF_TOKEN "${HF_TOKEN}" CACHE STRING "HF token" FORCE)
    elseif(DEFINED ENV{HF_TOKEN} AND NOT "$ENV{HF_TOKEN}" STREQUAL "")
        set(HF_TOKEN "$ENV{HF_TOKEN}" CACHE STRING "HF token" FORCE)
    else()
        message(STATUS "Hugging Face token is not set in environment. Checking .netrc in home ...")
    endif()
else()
    message(STATUS "LLM model downloads are disabled (DOWNLOAD_LLM_MODELS=OFF).")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND}  -E env "HF_TOKEN=${HF_TOKEN}" ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/py/download_resources.py
        --requirements-file
        ${CMAKE_CURRENT_LIST_DIR}/../py/requirements.json
        --download-dir
        ${DOWNLOADS_DIR}
        --llm-framework
        ${LLM_FRAMEWORK}
        --download-models
        ${DOWNLOAD_LLM_MODELS}
    RESULT_VARIABLE return_code)

# Release the lock:
message(STATUS "Releasing locked resource ${DOWNLOADS_DIR}")
file(LOCK ${DOWNLOADS_DIR} DIRECTORY RELEASE)

if (NOT return_code STREQUAL "0")
    message(FATAL_ERROR "Failed to download resources. Error code ${return_code}")
endif ()
