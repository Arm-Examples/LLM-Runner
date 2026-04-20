#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

include_guard(GLOBAL)

if("${LLM_FRAMEWORK}" STREQUAL "llama.cpp"
        AND "${TARGET_PLATFORM}"  STREQUAL "linux-aarch64")

  # Following block of code determines CMAKE_C_FLAGS / CMAKE_CXX_FLAGS to be used.
  # Preset names mirror upstream ggml Linux/Android CPU presets; suffixes encode
  # incremental feature bundles for Armv8.x/Armv9.x. Some mappings are adjusted
  # to keep SVE disabled due to the known upstream issue.
  # Source:https://github.com/ggml-org/llama.cpp/blob/master/ggml/src/CMakeLists.txt#L390

  set(_allowed_arches
          Armv8.2_1
          Armv8.2_2
          Armv8.2_3
          Armv8.2_4
          Armv8.6_1
          Armv9.2_1)

  if(NOT DEFINED CPU_ARCH)
    list(JOIN _allowed_arches ", " _allowed_str)
    message(FATAL_ERROR
            "CPU_ARCH is required to avoid enabling SVE in llama.cpp. "
            "Allowed values: ${_allowed_str}.")
  endif()

  list(FIND _allowed_arches "${CPU_ARCH}" _idx)
  if(_idx EQUAL -1)
    list(JOIN _allowed_arches ", " _allowed_str)
    message(FATAL_ERROR
            "Invalid CPU_ARCH='${CPU_ARCH}'. CPU_ARCH is required to avoid enabling "
            "SVE in llama.cpp. Allowed values: ${_allowed_str}.")
  endif()

  if(CPU_ARCH STREQUAL "Armv8.2_1")
    # ggml armv8.2_1 preset: DOTPROD only.
    set(_march "armv8.2-a+dotprod")
  elseif(CPU_ARCH STREQUAL "Armv8.2_2")
    # ggml armv8.2_2 preset: DOTPROD + FP16.
    set(_march "armv8.2-a+dotprod+fp16")
  elseif(CPU_ARCH STREQUAL "Armv8.2_3")
    # Custom preset: diverges from ggml's armv8.2_3 by removing SVE
    # Use this variant to expose the int8 matrix-multiply extension (+i8mm)
    set(_march "armv8.2-a+dotprod+i8mm")
  elseif(CPU_ARCH STREQUAL "Armv8.2_4")
    # Custom preset: diverges from ggml's preset ladder.
    # This keeps SVE disabled while exposing both FP16 and I8MM, which is useful
    # for KleidiAI / quantized inference paths.
    set(_march "armv8.2-a+dotprod+fp16+i8mm")
  elseif(CPU_ARCH STREQUAL "Armv8.6_1")
    # Custom preset: Based on ggml's armv8.6_1 preset with SVE disabled
    set(_march "armv8.6-a+dotprod+fp16+i8mm")
  elseif(CPU_ARCH STREQUAL "Armv9.2_1")
    # Custom preset: Based on ggml's armv9.2_1 preset with SVE disabled
    set(_march "armv9.2-a+dotprod+fp16+nosve+i8mm+sme")
  else()
    list(JOIN _allowed_arches ", " _allowed_str)
    message(FATAL_ERROR
            "CPU_ARCH is set to an invalid value. CPU_ARCH is required to avoid "
            "enabling SVE in llama.cpp on this backend. Allowed values: ${_allowed_str}.")
  endif()

  # Avoid stacking multiple -march flags if one is already defined.
  foreach(_flags_var CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    string(REGEX REPLACE "(^| )-march=[^ ]+" "" _cleaned_flags "${${_flags_var}}")
    string(STRIP "${_cleaned_flags}" _cleaned_flags)
    if(_cleaned_flags STREQUAL "")
      set(${_flags_var} "-march=${_march}")
    else()
      set(${_flags_var} "${_cleaned_flags} -march=${_march}")
    endif()
  endforeach()

  message(STATUS "CPU_ARCH=${CPU_ARCH} -> -march=${_march}")

endif()
