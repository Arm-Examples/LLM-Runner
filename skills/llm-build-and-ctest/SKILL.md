---
name: llm-build-and-ctest
description: Configure, build, and run CTest for this repository using CMake presets (native, x-android-aarch64, x-linux-aarch64), including common cache flags (LLM_FRAMEWORK, BUILD_JNI_LIB, USE_KLEIDIAI, BUILD_BENCHMARK, CPU_ARCH) and failure triage steps. Use when asked to run tests, reproduce build/CI failures, or diagnose CMake/CTest/JNI issues for this project.
---

# LLM build and ctest

## Workflow

### 1) Configure (preset + optional flags)

Use one of the repo presets in `CMakePresets.json`:

```sh
cmake --preset=native -B build
```

Common configuration flags:

```sh
# Select backend
cmake --preset=native -B build -DLLM_FRAMEWORK=llama.cpp
cmake --preset=native -B build -DLLM_FRAMEWORK=onnxruntime-genai
cmake --preset=native -B build -DLLM_FRAMEWORK=mediapipe
cmake --preset=native -B build -DLLM_FRAMEWORK=mnn
```

```sh
# Disable JNI (faster iteration; avoids requiring JDK)
cmake --preset=native -B build -DBUILD_JNI_LIB=OFF
```

```sh
# Disable KleidiAI optimizations (for A/B testing)
cmake --preset=native -B build -DUSE_KLEIDIAI=OFF
```

```sh
# AArch64 ISA selection (llama.cpp + linux-aarch64 target only)
cmake --preset=native -B build -DCPU_ARCH=Armv8.2_4
```

### 2) Build

```sh
cmake --build ./build
```

### 3) Run tests (CTest)

```sh
ctest --test-dir ./build --output-on-failure
```

Run verbose and/or a subset:

```sh
ctest --test-dir ./build -V
ctest --test-dir ./build -N
ctest --test-dir ./build -R llm-cpp-ctest -V
```

### 4) Diagnose common failures

- Handle downloads: configure runs a downloads step (`scripts/cmake/download-resources.cmake` → `scripts/py/download_resources.py`); set `HF_TOKEN` (or `~/.netrc` for `huggingface.co`) for gated models.
- Check logs: read `build/CMakeFiles/CMakeOutput.log` and `build/CMakeFiles/CMakeError.log`.
- Isolate JNI: retry with `-DBUILD_JNI_LIB=OFF` to separate C++ failures from Java/JNI toolchain issues.
- Confirm cache: inspect `build/CMakeCache.txt` for `LLM_FRAMEWORK`, `BUILD_JNI_LIB`, `USE_KLEIDIAI`, `CPU_ARCH`.

## Optional automation

Run the bundled helper script:

```sh
bash skills/llm-build-and-ctest/scripts/run_build_and_ctest.sh native build
```

For deeper triage patterns, load `skills/llm-build-and-ctest/references/ctest-triage.md`.
