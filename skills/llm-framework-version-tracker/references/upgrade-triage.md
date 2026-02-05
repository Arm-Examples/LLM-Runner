<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Upgrade triage (framework bumps)

## General strategy

- Upgrade one framework at a time; keep each bump small and testable.
- Reconfigure from a clean state when in doubt (delete `build/`).
- Prefer isolating JNI during initial triage (`-DBUILD_JNI_LIB=OFF`) unless the bump is JNI-related.

## What to capture on failures

- `cmake --preset=native -B build ...` command and the exact cache flags used
- `build/CMakeCache.txt` (or the relevant pin variables)
- `ctest --test-dir ./build --output-on-failure` output
- If compilation fails: the first error, plus the include path / symbol name

## Common breakage zones

### `llama.cpp`

- Header/namespace changes: re-check includes under `src/cpp/frameworks/llama_cpp/`.
- Tokenization / EOS / chat-template differences can change outputs; update tests carefully (see output-validation notes in `skills/llm-add-model-support/`).
- Build flag changes: upstream CMake options occasionally rename; verify what this repo sets (KleidiAI flags, shared libs for Android).

### `onnxruntime` + `onnxruntime-genai`

- Compatibility: genai versions may require specific ORT versions.
- The staging workaround in `src/cpp/frameworks/onnxruntime_genai/CMakeLists.txt` is sensitive to upstream layout changes; if the include/lib expectations move, adjust the staging paths.
- If benchmarks fail, confirm `model_benchmark` links against the correct `onnxruntime` shared lib output.

### `mnn`

- CMake option surface can change; re-check the MNN build flags set in `src/cpp/frameworks/mnn/CMakeLists.txt`.
- Header paths may move; update `target_include_directories`.

## Proving the upgrade

- Run: `python3 scripts/dev/framework_versions.py --build-dir build` and include the output in the PR summary.
- Build + test at least the affected backend: `cmake --preset=native -B build -DLLM_FRAMEWORK=<backend> && cmake --build ./build && ctest --test-dir ./build --output-on-failure`

