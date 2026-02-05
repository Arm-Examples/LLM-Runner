<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# API change checklist (this repo)

## C++ API surface

- Update the public header: `src/cpp/interface/Llm.hpp`
- Prefer additive changes (new methods/params with defaults) over breaking changes
- Maintain invariants:
  - `LlmInit()` must be called before encode/decode paths
  - `NextToken()` returns `LLM::endToken` (`"<eos>"`) for stop tokens

## Implementation glue

- Update implementation: `src/cpp/Llm.cpp`
- If the change affects cross-thread/cancel behavior, review:
  - `LLM::CancellableNextToken()`, `LLM::Cancel()`, `LLM::StopGeneration()`
- If a change impacts resource lifetime, verify `~LLM()` and `FreeLlm()`

## Config schema changes

- Update parser: `src/cpp/config/LlmConfig.*`
- Update tests: `test/cpp/LlmConfigTest.cpp`
- Ensure new fields have safe defaults for existing configs

## Backend implementations

- If an API change adds capability requirements, ensure every backend either:
  - Implements it, or
  - Fails clearly with a good error message
- Verify modality support checks remain correct (text/image)

## JNI bindings (if BUILD_JNI_LIB=ON)

- Update JNI: `src/cpp/LlmJni.cpp` and the Java interface under `src/java/`
- If changing signatures, ensure tests still run (see JNI tests wired in `test/CMakeLists.txt`)
- If JNI is not relevant to the feature, validate at least one build with `-DBUILD_JNI_LIB=OFF` to isolate issues

## Proof

- Build + test:
  - `cmake --preset=native -B build`
  - `cmake --build ./build`
  - `ctest --test-dir ./build --output-on-failure`

