<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Benchmark notes (this repo)

## Build flags

- Enable benchmark targets: `-DBUILD_BENCHMARK=ON`
- Select backend: `-DLLM_FRAMEWORK=<llama.cpp|onnxruntime-genai|mediapipe|mnn>`
- Disable JNI during iteration: `-DBUILD_JNI_LIB=OFF`

## Running `llm-bench-cli`

`llm-bench-cli` measures encode/decode performance with a consistent harness and reports metrics like throughput, latency, and TTFT.

Example:

```sh
./build/bin/llm-bench-cli -m <model_or_config_path> -i 128 -o 64 -c 2048 -t 4 -n 3 -w 1
```

Key args:
- `-m`: model/config path (backend-specific; see `README.md` for examples)
- `-i` / `-o`: input/output token counts
- `-c`: context size (must be one of: 128, 256, 512, 1024, 2048)
- `-t`: threads
- `-n`: iterations
- `-w`: warmup iterations (excluded from stats)

## Common runtime pitfalls

- **Models not present**: configure triggers downloads; gated models require `HF_TOKEN` or `~/.netrc`.
- **Shared libraries** (especially on-device/Android): keep `llm-bench-cli` and required backend `.so` files in the same directory, or set `LD_LIBRARY_PATH` appropriately.
- **OpenMP on Android**: if built with OpenMP, ensure required runtime libraries are present (see `TROUBLESHOOTING.md`).

## When changing benchmark code

- Keep output stable and parse-friendly (avoid cosmetic churn unless required).
- Avoid making benchmarks implicitly download things; keep downloads at configure-time where possible.
- Validate: build with `-DBUILD_BENCHMARK=ON` and run `--help` plus at least one real invocation.
