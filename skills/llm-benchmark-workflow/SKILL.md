---
name: llm-benchmark-workflow
description: Build and run the benchmarking tools in this repository (including arm-llm-bench-cli) across supported backends, and triage benchmark build/runtime issues (shared libs placement, model paths, threads/tokens, JNI off). Use when changing benchmark code, adding metrics, comparing performance, or verifying benchmark binaries for a backend.
---

# LLM benchmark workflow

Benchmarks are gated behind `-DBUILD_BENCHMARK=ON` and produce executables under `build/bin/`.

## Workflow

### 1) Configure + build benchmark binaries

```sh
cmake --preset=native -B build -DBUILD_BENCHMARK=ON
cmake --build ./build
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

### 2) Run `arm-llm-bench-cli`

`arm-llm-bench-cli` is backend-agnostic; it infers backend behavior from the model/config file you pass.

```sh
./build/bin/arm-llm-bench-cli -m <model_or_config_path> -i 128 -o 64 -t 4 -n 3 -w 1
```

For specifics and runtime pitfalls, load `skills/llm-benchmark-workflow/references/bench-notes.md`.

### 3) Quick smoke checks

- Confirm binary exists and prints help:
  - `bash skills/llm-benchmark-workflow/scripts/bench_smoke.sh build`
- Confirm CTest still passes after benchmark changes:
  - `ctest --test-dir ./build --output-on-failure`
