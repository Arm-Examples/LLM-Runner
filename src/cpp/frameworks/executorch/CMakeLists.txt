#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

project(executorch-wrapper
        DESCRIPTION "ExecuTorch wrapper interface implementation"
        LANGUAGES C CXX ASM)

include(FetchContent)
include(python-deps)
llm_ensure_python_requirements(
        "${LLM_SOURCE_DIR}/scripts/py/requirements-executorch.txt"
        yaml
        torchgen)

# --------------------------------------------------------------------
# ExecuTorch build
# --------------------------------------------------------------------
set(EXECUTORCH_SRC_DIR "${CMAKE_BINARY_DIR}/_deps/executorch"
        CACHE PATH "Path where ExecuTorch repo should be cloned")

set(EXECUTORCH_GIT_URL "https://github.com/pytorch/executorch.git"
        CACHE STRING "Git URL for ExecuTorch repo")

# Pinned release tag for deterministic runtime integration.
set(EXECUTORCH_GIT_TAG "v1.2.0"
        CACHE STRING "Git tag / commit SHA for ExecuTorch repo")

set(EXECUTORCH_BUILD_PRESET_FILE "${EXECUTORCH_SRC_DIR}/tools/cmake/preset/llm.cmake"
        CACHE FILEPATH "ExecuTorch preset file used to enable LLM runtime features")

# Text-path runtime support.
set(EXECUTORCH_BUILD_XNNPACK ON)
set(EXECUTORCH_BUILD_EXTENSION_DATA_LOADER ON)
set(EXECUTORCH_BUILD_EXTENSION_MODULE ON)
set(EXECUTORCH_BUILD_EXTENSION_TENSOR ON)
set(EXECUTORCH_BUILD_EXTENSION_LLM ON)
set(EXECUTORCH_BUILD_EXTENSION_LLM_RUNNER ON)
set(EXECUTORCH_BUILD_EXTENSION_RUNNER_UTIL ON)
# Enable the upstream LLM runtime kernels required by the current ExecuTorch
# text-model path.
set(EXECUTORCH_BUILD_KERNELS_LLM ON)
set(EXECUTORCH_BUILD_KERNELS_OPTIMIZED ON)
set(EXECUTORCH_BUILD_KERNELS_QUANTIZED ON)
set(EXECUTORCH_BUILD_PTHREADPOOL ON)

# This integration only targets the native runtime and C++ runner libraries.
set(EXECUTORCH_BUILD_PYBIND OFF)

if (LLM_LOG_LEVEL STREQUAL "DEBUG")
    set(EXECUTORCH_MAPPED_LOG_LEVEL Debug)
elseif(LLM_LOG_LEVEL STREQUAL "INFO")
    set(EXECUTORCH_MAPPED_LOG_LEVEL Info)
else()
    # ExecuTorch does not expose a Warn level.
    set(EXECUTORCH_MAPPED_LOG_LEVEL Error)
endif()
set(EXECUTORCH_LOG_LEVEL ${EXECUTORCH_MAPPED_LOG_LEVEL}
        CACHE STRING "ExecuTorch log level mapped from LLM_LOG_LEVEL" FORCE)

# ExecuTorch's codegen modules expect both the repo root and src tree on
# PYTHONPATH during configure-time code generation.
if (DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
    set(ENV{PYTHONPATH} "${EXECUTORCH_SRC_DIR}/src:${EXECUTORCH_SRC_DIR}:$ENV{PYTHONPATH}")
else()
    set(ENV{PYTHONPATH} "${EXECUTORCH_SRC_DIR}/src:${EXECUTORCH_SRC_DIR}")
endif()

FetchContent_Declare(executorch
        GIT_REPOSITORY ${EXECUTORCH_GIT_URL}
        GIT_TAG        ${EXECUTORCH_GIT_TAG}
        GIT_SHALLOW    1
        GIT_SUBMODULES_RECURSE TRUE
        SOURCE_DIR     ${EXECUTORCH_SRC_DIR}
        EXCLUDE_FROM_ALL)

FetchContent_GetProperties(executorch)
if (NOT executorch_POPULATED)
    FetchContent_Populate(executorch)
endif()

set(_tokenizers_cmake_paths
        "${EXECUTORCH_SRC_DIR}/extension/llm/tokenizers/CMakeLists.txt"
        "${executorch_SOURCE_DIR}/extension/llm/tokenizers/CMakeLists.txt"
        "${executorch_BINARY_DIR}/extension/llm/tokenizers/CMakeLists.txt")
foreach(_tokenizers_cmake IN LISTS _tokenizers_cmake_paths)
    if (EXISTS "${_tokenizers_cmake}")
        file(READ "${_tokenizers_cmake}" _tokenizers_contents)
        set(_tokenizers_original "${_tokenizers_contents}")
        if (NOT _tokenizers_contents MATCHES "LLM_PATCH_SKIP_TOKENIZERS_ABSEIL")
            string(REPLACE
                    [=[add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/abseil-cpp)]=]
                    [=[# LLM_PATCH_SKIP_TOKENIZERS_ABSEIL
if (TARGET log_internal_message OR TARGET absl_log OR TARGET absl::base OR TARGET absl_base)
  message(STATUS "Skipping ExecuTorch tokenizers abseil-cpp; targets already defined")
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/abseil-cpp)
endif()]=]
                    _tokenizers_contents
                    "${_tokenizers_contents}")
        endif()
        if (NOT _tokenizers_contents MATCHES "LLM_PATCH_SKIP_TOKENIZERS_RE2")
            string(REPLACE
                    [=[add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/re2)]=]
                    [=[# LLM_PATCH_SKIP_TOKENIZERS_RE2
if (TARGET re2::re2 OR TARGET re2)
  message(STATUS "Skipping ExecuTorch tokenizers re2; target already defined")
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/re2)
endif()]=]
                    _tokenizers_contents
                    "${_tokenizers_contents}")
        endif()
        if (_tokenizers_contents MATCHES "add_subdirectory\\([^\\)]*abseil-cpp[^\\)]*\\)"
                AND NOT _tokenizers_contents MATCHES "LLM_PATCH_SKIP_TOKENIZERS_ABSEIL")
            string(REGEX REPLACE
                    "add_subdirectory\\(([^\\)]*abseil-cpp[^\\)]*)\\)"
                    "# LLM_PATCH_SKIP_TOKENIZERS_ABSEIL\n"
                    "if (TARGET log_internal_message OR TARGET absl_log OR TARGET absl::base OR TARGET absl_base)\n"
                    "  message(STATUS \"Skipping ExecuTorch tokenizers abseil-cpp; targets already defined\")\n"
                    "else()\n"
                    "  add_subdirectory(\\1)\n"
                    "endif()"
                    _tokenizers_contents
                    "${_tokenizers_contents}")
        endif()
        if (_tokenizers_contents MATCHES "add_subdirectory\\([^\\)]*re2[^\\)]*\\)"
                AND NOT _tokenizers_contents MATCHES "LLM_PATCH_SKIP_TOKENIZERS_RE2")
            string(REGEX REPLACE
                    "add_subdirectory\\(([^\\)]*re2[^\\)]*)\\)"
                    "# LLM_PATCH_SKIP_TOKENIZERS_RE2\n"
                    "if (TARGET re2::re2 OR TARGET re2)\n"
                    "  message(STATUS \"Skipping ExecuTorch tokenizers re2; target already defined\")\n"
                    "else()\n"
                    "  add_subdirectory(\\1)\n"
                    "endif()"
                    _tokenizers_contents
                    "${_tokenizers_contents}")
        endif()
        if (NOT _tokenizers_contents MATCHES "LLM_PATCH_PCRE2_ANDROID")
            string(REPLACE
                    [=[add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/pcre2)]=]
                    [=[# LLM_PATCH_PCRE2_ANDROID
set(_llm_pcre2_build_shared_libs_set FALSE)
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  if (DEFINED BUILD_SHARED_LIBS)
    set(_llm_pcre2_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(_llm_pcre2_build_shared_libs_set TRUE)
  endif()
  set(BUILD_SHARED_LIBS OFF)
  set(PCRE2_BUILD_SHARED_LIBS OFF)
  set(PCRE2_BUILD_SHARED_LIBS OFF CACHE BOOL "Disable PCRE2 shared libs on Android" FORCE)
endif()
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/pcre2)
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  if (_llm_pcre2_build_shared_libs_set)
    set(BUILD_SHARED_LIBS "${_llm_pcre2_build_shared_libs}")
  else()
    unset(BUILD_SHARED_LIBS)
  endif()
  unset(PCRE2_BUILD_SHARED_LIBS CACHE)
  unset(PCRE2_BUILD_SHARED_LIBS)
  unset(_llm_pcre2_build_shared_libs)
  unset(_llm_pcre2_build_shared_libs_set)
endif()]=]
                    _tokenizers_contents
                    "${_tokenizers_contents}")
        endif()
        if (_tokenizers_contents MATCHES "add_subdirectory\\([^\\)]*pcre2[^\\)]*\\)"
                AND NOT _tokenizers_contents MATCHES "LLM_PATCH_PCRE2_ANDROID")
            string(REGEX REPLACE
                    "add_subdirectory\\(([^\\)]*pcre2[^\\)]*)\\)"
                    "# LLM_PATCH_PCRE2_ANDROID\n"
                    "set(_llm_pcre2_build_shared_libs_set FALSE)\n"
                    "if (CMAKE_SYSTEM_NAME STREQUAL \"Android\")\n"
                    "  if (DEFINED BUILD_SHARED_LIBS)\n"
                    "    set(_llm_pcre2_build_shared_libs \"${BUILD_SHARED_LIBS}\")\n"
                    "    set(_llm_pcre2_build_shared_libs_set TRUE)\n"
                    "  endif()\n"
                    "  set(BUILD_SHARED_LIBS OFF)\n"
                    "  set(PCRE2_BUILD_SHARED_LIBS OFF)\n"
                    "  set(PCRE2_BUILD_SHARED_LIBS OFF CACHE BOOL \"Disable PCRE2 shared libs on Android\" FORCE)\n"
                    "endif()\n"
                    "add_subdirectory(\\1)\n"
                    "if (CMAKE_SYSTEM_NAME STREQUAL \"Android\")\n"
                    "  if (_llm_pcre2_build_shared_libs_set)\n"
                    "    set(BUILD_SHARED_LIBS \"${_llm_pcre2_build_shared_libs}\")\n"
                    "  else()\n"
                    "    unset(BUILD_SHARED_LIBS)\n"
                    "  endif()\n"
                    "  unset(PCRE2_BUILD_SHARED_LIBS CACHE)\n"
                    "  unset(PCRE2_BUILD_SHARED_LIBS)\n"
                    "  unset(_llm_pcre2_build_shared_libs)\n"
                    "  unset(_llm_pcre2_build_shared_libs_set)\n"
                    "endif()"
                    _tokenizers_contents
                    "${_tokenizers_contents}")
        endif()
        if (NOT _tokenizers_contents STREQUAL _tokenizers_original)
            file(WRITE "${_tokenizers_cmake}" "${_tokenizers_contents}")
        endif()
        unset(_tokenizers_original)
        unset(_tokenizers_contents)
    endif()
endforeach()
unset(_tokenizers_cmake_paths)

set(_pcre2_cmake_paths
        "${EXECUTORCH_SRC_DIR}/extension/llm/tokenizers/third-party/pcre2/CMakeLists.txt"
        "${executorch_SOURCE_DIR}/extension/llm/tokenizers/third-party/pcre2/CMakeLists.txt"
        "${executorch_BINARY_DIR}/extension/llm/tokenizers/third-party/pcre2/CMakeLists.txt")
foreach(_pcre2_cmake IN LISTS _pcre2_cmake_paths)
    if (EXISTS "${_pcre2_cmake}")
        file(READ "${_pcre2_cmake}" _pcre2_contents)
        set(_pcre2_original "${_pcre2_contents}")
        if (NOT _pcre2_contents MATCHES "LLM_PATCH_PCRE2_LLD_UNDEFINED_VERSION")
            string(REPLACE
                    [=[file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/test-map-file.sym)
file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/test-map-file-broken.sym)]=]
                    [=[# LLM_PATCH_PCRE2_LLD_UNDEFINED_VERSION
if (HAVE_VSCRIPT)
  set(PCRE2_VSCRIPT_EXTRA_FLAGS "")
  if (CMAKE_C_COMPILER_LINKER_ID STREQUAL "LLD"
      OR CMAKE_CXX_COMPILER_LINKER_ID STREQUAL "LLD"
      OR CMAKE_LINKER MATCHES "ld.lld")
    set(PCRE2_VSCRIPT_EXTRA_FLAGS "-Wl,--undefined-version")
  endif()
endif()
file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/test-map-file.sym)
file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/test-map-file-broken.sym)]=]
                    _pcre2_contents
                    "${_pcre2_contents}")
            string(REPLACE
                    [=[target_link_options(pcre2-8-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-8.sym)]=]
                    [=[target_link_options(pcre2-8-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-8.sym ${PCRE2_VSCRIPT_EXTRA_FLAGS})]=]
                    _pcre2_contents
                    "${_pcre2_contents}")
            string(REPLACE
                    [=[target_link_options(pcre2-posix-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-posix.sym)]=]
                    [=[target_link_options(pcre2-posix-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-posix.sym ${PCRE2_VSCRIPT_EXTRA_FLAGS})]=]
                    _pcre2_contents
                    "${_pcre2_contents}")
            string(REPLACE
                    [=[target_link_options(pcre2-16-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-16.sym)]=]
                    [=[target_link_options(pcre2-16-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-16.sym ${PCRE2_VSCRIPT_EXTRA_FLAGS})]=]
                    _pcre2_contents
                    "${_pcre2_contents}")
            string(REPLACE
                    [=[target_link_options(pcre2-32-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-32.sym)]=]
                    [=[target_link_options(pcre2-32-shared PRIVATE -Wl,${VSCRIPT_FLAG},${PROJECT_SOURCE_DIR}/src/libpcre2-32.sym ${PCRE2_VSCRIPT_EXTRA_FLAGS})]=]
                    _pcre2_contents
                    "${_pcre2_contents}")
        endif()
        if (NOT _pcre2_contents STREQUAL _pcre2_original)
            file(WRITE "${_pcre2_cmake}" "${_pcre2_contents}")
        endif()
        unset(_pcre2_original)
        unset(_pcre2_contents)
    endif()
endforeach()
unset(_pcre2_cmake_paths)

set(_cpuinfo_cmake_paths
        "${executorch_SOURCE_DIR}/backends/xnnpack/third-party/cpuinfo/CMakeLists.txt"
        "${executorch_BINARY_DIR}/backends/xnnpack/third-party/cpuinfo/CMakeLists.txt")
foreach(_cpuinfo_cmake IN LISTS _cpuinfo_cmake_paths)
    if (EXISTS "${_cpuinfo_cmake}")
        file(READ "${_cpuinfo_cmake}" _cpuinfo_contents)
        if (NOT _cpuinfo_contents MATCHES "LLM_PATCH_SKIP_CPUINFO_TARGETS")
            string(REPLACE
                    [=[CMAKE_MINIMUM_REQUIRED(VERSION 3.5 FATAL_ERROR)]=]
                    [=[CMAKE_MINIMUM_REQUIRED(VERSION 3.5 FATAL_ERROR)

# LLM_PATCH_SKIP_CPUINFO_TARGETS
if (TARGET cpuinfo)
  message(STATUS "Skipping ExecuTorch bundled cpuinfo; target already defined")
  return()
endif()]=]
                    _cpuinfo_contents
                    "${_cpuinfo_contents}")
            file(WRITE "${_cpuinfo_cmake}" "${_cpuinfo_contents}")
        endif()
        unset(_cpuinfo_contents)
    endif()
endforeach()
unset(_cpuinfo_cmake_paths)

set(_third_party_cmake_paths
        "${EXECUTORCH_SRC_DIR}/third-party/CMakeLists.txt"
        "${executorch_SOURCE_DIR}/third-party/CMakeLists.txt"
        "${executorch_BINARY_DIR}/third-party/CMakeLists.txt")
foreach(_third_party_cmake IN LISTS _third_party_cmake_paths)
    if (EXISTS "${_third_party_cmake}")
        file(READ "${_third_party_cmake}" _third_party_contents)
        set(_third_party_original "${_third_party_contents}")
        if (NOT _third_party_contents MATCHES "LLM_PATCH_GUARD_FLATC")
            string(REPLACE
                    [=[add_executable(flatc IMPORTED GLOBAL)]=]
                    [=[# LLM_PATCH_GUARD_FLATC
if (TARGET flatc)
  message(STATUS "Skipping ExecuTorch flatc target; already defined")
else()
  add_executable(flatc IMPORTED GLOBAL)
endif()]=]
                    _third_party_contents
                    "${_third_party_contents}")
        endif()
        if (NOT _third_party_contents MATCHES "LLM_PATCH_GUARD_FLATCC")
            string(REPLACE
                    [=[add_executable(flatcc_cli IMPORTED GLOBAL)]=]
                    [=[# LLM_PATCH_GUARD_FLATCC
if (TARGET flatcc_cli)
  message(STATUS "Skipping ExecuTorch flatcc_cli target; already defined")
else()
  add_executable(flatcc_cli IMPORTED GLOBAL)
endif()]=]
                    _third_party_contents
                    "${_third_party_contents}")
        endif()
        if (NOT _third_party_contents STREQUAL _third_party_original)
            file(WRITE "${_third_party_cmake}" "${_third_party_contents}")
        endif()
        unset(_third_party_original)
        unset(_third_party_contents)
    endif()
endforeach()
unset(_third_party_cmake_paths)

set(EXECUTORCH_FLATC_EXECUTABLE
        "${executorch_BINARY_DIR}/third-party/flatc_ep/bin/flatc"
        CACHE FILEPATH "Path to ExecuTorch flatc executable" FORCE)

set(_schema_cmake_paths
        "${EXECUTORCH_SRC_DIR}/schema/CMakeLists.txt"
        "${executorch_SOURCE_DIR}/schema/CMakeLists.txt"
        "${executorch_BINARY_DIR}/schema/CMakeLists.txt")
foreach(_schema_cmake IN LISTS _schema_cmake_paths)
    if (EXISTS "${_schema_cmake}")
        file(READ "${_schema_cmake}" _schema_contents)
        set(_schema_original "${_schema_contents}")
        if (NOT _schema_contents MATCHES "LLM_PATCH_EXECUTORCH_FLATC")
            string(REPLACE
                    [=[COMMAND flatc --cpp --cpp-std c++11 --gen-mutable --scoped-enums -o]=]
                    [=[# LLM_PATCH_EXECUTORCH_FLATC
    COMMAND ${EXECUTORCH_FLATC_EXECUTABLE} --cpp --cpp-std c++11 --gen-mutable --scoped-enums -o]=]
                    _schema_contents
                    "${_schema_contents}")
            string(REPLACE
                    [=[DEPENDS flatc ${_schema_srcs}]=]
                    [=[DEPENDS flatbuffers_ep ${_schema_srcs}]=]
                    _schema_contents
                    "${_schema_contents}")
        endif()
        if (NOT _schema_contents STREQUAL _schema_original)
            file(WRITE "${_schema_cmake}" "${_schema_contents}")
        endif()
        unset(_schema_original)
        unset(_schema_contents)
    endif()
endforeach()
unset(_schema_cmake_paths)

set(_custom_ops_cmake_paths
        "${EXECUTORCH_SRC_DIR}/extension/llm/custom_ops/CMakeLists.txt"
        "${executorch_SOURCE_DIR}/extension/llm/custom_ops/CMakeLists.txt"
        "${executorch_BINARY_DIR}/extension/llm/custom_ops/CMakeLists.txt")
foreach(_custom_ops_cmake IN LISTS _custom_ops_cmake_paths)
    if (EXISTS "${_custom_ops_cmake}")
        file(READ "${_custom_ops_cmake}" _custom_ops_contents)
        if (NOT _custom_ops_contents MATCHES "LLM_PATCH_CUSTOM_OPS_CXX_FHT")
            string(REPLACE
                    [=[add_library(custom_ops ${_custom_ops__srcs})]=]
                    [=[add_library(custom_ops ${_custom_ops__srcs})
# LLM_PATCH_CUSTOM_OPS_CXX_FHT
foreach(_custom_op_src IN LISTS _custom_ops__srcs)
  if (_custom_op_src MATCHES "fht_(avx|neon)\\.c$")
    set_source_files_properties("${_custom_op_src}" PROPERTIES LANGUAGE CXX)
  endif()
endforeach()
unset(_custom_op_src)]=]
                    _custom_ops_contents
                    "${_custom_ops_contents}")
            file(WRITE "${_custom_ops_cmake}" "${_custom_ops_contents}")
        endif()
        unset(_custom_ops_contents)
    endif()
endforeach()
unset(_custom_ops_cmake_paths)

set(_xnnpack_cmake_paths
        "${EXECUTORCH_SRC_DIR}/backends/xnnpack/CMakeLists.txt"
        "${executorch_SOURCE_DIR}/backends/xnnpack/CMakeLists.txt"
        "${executorch_BINARY_DIR}/backends/xnnpack/CMakeLists.txt")
foreach(_xnnpack_cmake IN LISTS _xnnpack_cmake_paths)
    if (EXISTS "${_xnnpack_cmake}")
        file(READ "${_xnnpack_cmake}" _xnnpack_contents)
        set(_xnnpack_original "${_xnnpack_contents}")
        if (NOT _xnnpack_contents MATCHES "LLM_PATCH_EXECUTORCH_FLATC_XNNPACK")
            string(REPLACE
                    [=[flatc --cpp --cpp-std c++11 --scoped-enums -o]=]
                    [=[# LLM_PATCH_EXECUTORCH_FLATC_XNNPACK
    ${EXECUTORCH_FLATC_EXECUTABLE} --cpp --cpp-std c++11 --scoped-enums -o]=]
                    _xnnpack_contents
                    "${_xnnpack_contents}")
            string(REPLACE
                    [=[DEPENDS flatc]=]
                    [=[DEPENDS flatbuffers_ep]=]
                    _xnnpack_contents
                    "${_xnnpack_contents}")
        endif()
        if (NOT _xnnpack_contents STREQUAL _xnnpack_original)
            file(WRITE "${_xnnpack_cmake}" "${_xnnpack_contents}")
        endif()
        unset(_xnnpack_original)
        unset(_xnnpack_contents)
    endif()
endforeach()
unset(_xnnpack_cmake_paths)

set(_flat_tensor_cmake_paths
        "${EXECUTORCH_SRC_DIR}/extension/flat_tensor/serialize/CMakeLists.txt"
        "${executorch_SOURCE_DIR}/extension/flat_tensor/serialize/CMakeLists.txt"
        "${executorch_BINARY_DIR}/extension/flat_tensor/serialize/CMakeLists.txt")
foreach(_flat_tensor_cmake IN LISTS _flat_tensor_cmake_paths)
    if (EXISTS "${_flat_tensor_cmake}")
        file(READ "${_flat_tensor_cmake}" _flat_tensor_contents)
        set(_flat_tensor_original "${_flat_tensor_contents}")
        if (NOT _flat_tensor_contents MATCHES "LLM_PATCH_EXECUTORCH_FLATC_FLAT_TENSOR")
            string(REPLACE
                    [=[COMMAND flatc --cpp --cpp-std c++11 --gen-mutable --scoped-enums -o]=]
                    [=[# LLM_PATCH_EXECUTORCH_FLATC_FLAT_TENSOR
    COMMAND ${EXECUTORCH_FLATC_EXECUTABLE} --cpp --cpp-std c++11 --gen-mutable --scoped-enums -o]=]
                    _flat_tensor_contents
                    "${_flat_tensor_contents}")
            string(REPLACE
                    [=[DEPENDS flatc ${_schema_srcs}]=]
                    [=[DEPENDS flatbuffers_ep ${_schema_srcs}]=]
                    _flat_tensor_contents
                    "${_flat_tensor_contents}")
        endif()
        if (NOT _flat_tensor_contents STREQUAL _flat_tensor_original)
            file(WRITE "${_flat_tensor_cmake}" "${_flat_tensor_contents}")
        endif()
        unset(_flat_tensor_original)
        unset(_flat_tensor_contents)
    endif()
endforeach()
unset(_flat_tensor_cmake_paths)

set(_add_sub_impl
        "${executorch_SOURCE_DIR}/kernels/optimized/cpu/op_add_sub_impl.h")
if (EXISTS "${_add_sub_impl}")
    file(READ "${_add_sub_impl}" _add_sub_contents)
    if (_add_sub_contents MATCHES "val_alpha \\* val_b"
            AND NOT _add_sub_contents MATCHES "alpha_vec")
        string(REPLACE
                "            return val_a + val_alpha * val_b;"
                "            const auto alpha_vec = decltype(val_b)(val_alpha);\n            return val_a + alpha_vec * val_b;"
                _add_sub_contents
                "${_add_sub_contents}")
        file(WRITE "${_add_sub_impl}" "${_add_sub_contents}")
    endif()
    unset(_add_sub_contents)
endif()
unset(_add_sub_impl)

set(_preset_default
        "${executorch_SOURCE_DIR}/tools/cmake/preset/default.cmake")
if (EXISTS "${_preset_default}")
    file(READ "${_preset_default}" _preset_contents)
    if (_preset_contents MATCHES "EXECUTORCH_BUILD_KERNELS_LLM REQUIRES"
            AND NOT _preset_contents MATCHES "LLM_PATCH_DISABLE_KERNELS_OPTIMIZED_REQUIRE")
        string(REPLACE
                "check_required_options_on(\n  IF_ON EXECUTORCH_BUILD_KERNELS_LLM REQUIRES\n  EXECUTORCH_BUILD_KERNELS_OPTIMIZED\n)\n"
                "# LLM_PATCH_DISABLE_KERNELS_OPTIMIZED_REQUIRE\n"
                _preset_contents
                "${_preset_contents}")
        file(WRITE "${_preset_default}" "${_preset_contents}")
    endif()
    unset(_preset_contents)
endif()
unset(_preset_default)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    # PCRE2's shared build enables version scripts that reference _init/_fini,
    # which are not defined for Android's linker environment.
    set(PCRE2_BUILD_SHARED_LIBS OFF
            CACHE BOOL "Disable PCRE2 shared libraries on Android" FORCE)
endif()

# Avoid clashing with the top-level nlohmann_json target that this project
# already defines for config parsing.
set(NLOHMANN_JSON_TARGET_NAME executorch_nlohmann_json)
add_subdirectory(${executorch_SOURCE_DIR} ${executorch_BINARY_DIR})
unset(NLOHMANN_JSON_TARGET_NAME)

if (TARGET custom_ops)
    if (MSVC)
        target_compile_options(custom_ops PRIVATE "/FIATen/cpu/vec/functional.h")
    else()
        target_compile_options(custom_ops PRIVATE "-include" "ATen/cpu/vec/functional.h")
    endif()
endif()

set(EXECUTORCH_REQUIRED_TARGETS
        executorch
        executorch::backends
        executorch::extensions
        extension_llm_runner
        extension_runner_util)

if (TARGET extension_threadpool)
    list(APPEND EXECUTORCH_REQUIRED_TARGETS extension_threadpool)
endif()

set(EXECUTORCH_RUNTIME_KERNEL_TARGETS
        quantized_ops_lib)

if (TARGET optimized_native_cpu_ops_lib)
    list(APPEND EXECUTORCH_RUNTIME_KERNEL_TARGETS optimized_native_cpu_ops_lib)
endif()

set(EXECUTORCH_RUNTIME_WHOLE_ARCHIVE_TARGETS)
if (TARGET custom_ops)
    list(APPEND EXECUTORCH_RUNTIME_WHOLE_ARCHIVE_TARGETS "$<LINK_LIBRARY:WHOLE_ARCHIVE,custom_ops>")
endif()

set(REQUIRED_TARGETS
        ${EXECUTORCH_REQUIRED_TARGETS}
        ${EXECUTORCH_RUNTIME_KERNEL_TARGETS})

foreach(required_target IN LISTS REQUIRED_TARGETS)
    if (NOT TARGET ${required_target})
        message(FATAL_ERROR "${required_target} target not defined")
    endif()
endforeach()

set(_executorch_flatbuffers_include "${EXECUTORCH_SRC_DIR}/third-party/flatbuffers/include")
# Ensure ExecuTorch's flatbuffers headers are preferred over other build deps.
if (TARGET executorch_core)
    target_include_directories(executorch_core BEFORE PRIVATE ${_executorch_flatbuffers_include})
endif()
unset(_executorch_flatbuffers_include)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    if (TARGET cpuinfo)
        target_link_libraries(cpuinfo PUBLIC log)
    endif()
    if (TARGET extension_threadpool)
        target_link_libraries(extension_threadpool PUBLIC log)
        target_link_options(extension_threadpool PUBLIC "-llog")
    endif()
endif()

if (NOT TARGET llm-framework-deps)
    message(FATAL_ERROR "llm-framework-deps target not defined")
elseif (NOT TARGET llm-log)
    message(FATAL_ERROR "llm-log target not defined")
endif()

# ExecuTorch exposes a helper that adds the platform-specific whole-archive
# linker flags required by kernel registration libraries. Apply it to every
# runtime kernel target we link so future registration libraries can be added
# through the same list safely.
foreach(executorch_runtime_kernel_target IN LISTS EXECUTORCH_RUNTIME_KERNEL_TARGETS)
    if (TARGET ${executorch_runtime_kernel_target})
        executorch_target_link_options_shared_lib(${executorch_runtime_kernel_target})
    endif()
endforeach()

add_library(arm-llm-framework-executorch STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ExecuTorchImpl.cpp)

target_include_directories(arm-llm-framework-executorch PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(arm-llm-framework-executorch PUBLIC
        ${REQUIRED_TARGETS}
        ${EXECUTORCH_RUNTIME_WHOLE_ARCHIVE_TARGETS}
        llm-framework-deps
        llm-log)
