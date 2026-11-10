<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# rebar-ml benchmarking (placeholder)

This directory is reserved for a future rebar-ml benchmarking component focused on
**pipeline behaviour** (throughput/latency and end-to-end timings across a
connected node graph).

## Build (when implemented)

Benchmarks will be gated behind a separate CMake option:

```sh
cmake --preset=native -B build -DBUILD_REBAR_BENCHMARK=ON
cmake --build ./build
```

Until the benchmarking code is added, enabling the option will only configure a
placeholder target (`rebar_ml_benchmark_interface`).
