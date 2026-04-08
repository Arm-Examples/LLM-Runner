---
name: llm-benchmark-workflow
description: Build and run the benchmarking tools in this repository (including llm-bench-cli) across supported backends, and triage benchmark build/runtime issues (shared libs placement, model paths, threads/tokens, JNI off). Use when changing benchmark code, adding metrics, comparing performance, or verifying benchmark binaries for a backend.
---

# Benchmark workflow

Benchmarks are gated behind `-DBUILD_BENCHMARK=ON` and produce executables under `build/bin/`.

Windows note: if `python3` isn’t available, use `python` (or `py -3`) for the scripts below.

## Offline/build caveat

During configure, CMake may use FetchContent to download dependencies from the network. If your network is blocked, configure can fail before tests run. To enable network access when the process runs in a sandbox, see `skills/README.md`. Otherwise, run the build on your local machine (with network access) and share the resulting build path with the tool (for example: `build dir: /path/to/build`), or point CMake at pre-downloaded dependencies in your environment.

## Workflow

### 1) Configure + build benchmark binaries

```sh
cmake --preset=native -B build -DBUILD_BENCHMARK=ON
cmake --build ./build --parallel
```

Select a backend as needed:

```sh
cmake --preset=native -B build -DBUILD_BENCHMARK=ON -DLLM_FRAMEWORK=llama.cpp
cmake --preset=native -B build -DBUILD_BENCHMARK=ON -DLLM_FRAMEWORK=onnxruntime-genai
cmake --preset=native -B build -DBUILD_BENCHMARK=ON -DLLM_FRAMEWORK=mediapipe
cmake --preset=native -B build -DBUILD_BENCHMARK=ON -DLLM_FRAMEWORK=mnn
```

If you want to avoid Java/JNI setup during benchmark iteration:

```sh
cmake --preset=native -B build -DBUILD_BENCHMARK=ON -DBUILD_JNI_LIB=OFF
```

### 2) Run `llm-bench-cli`

`llm-bench-cli` is backend-agnostic; it infers backend behavior from the model/config file you pass.
Passing a config JSON is recommended because it encodes the backend and paths.

```sh
./build/bin/llm-bench-cli -m <model_or_config_path> -i 128 -o 64 -c 2048 -t 4 -n 3 -w 1
```

For specifics and runtime pitfalls, load `skills/llm-benchmark-workflow/references/bench-notes.md`.

### 3) Quick smoke checks

- Confirm binary exists and prints help:
  - `python3 skills/llm-benchmark-workflow/scripts/bench_smoke.py build`
- Confirm CTest still passes after benchmark changes:
  - `ctest --test-dir ./build --output-on-failure`
