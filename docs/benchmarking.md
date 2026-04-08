<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
SPDX-License-Identifier: Apache-2.0
-->

# Benchmarking

This guide shows how to capture repeatable performance numbers and where to use Arm Streamline and Perfetto for profiling, including SME-enabled runs.
These tools are actively under development and instructions below should be considered as indicative of workflow.

## 1) Define a baseline workload

Pick a representative prompt (and image input if using a multimodal model) and keep it constant across runs. Record:

- Model: `resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf`
- Prompt length / tokens
- Build configuration (preset, flags)
- Runtime flags (e.g., `GGML_KLEIDIAI_SME`)

## 2) Build for benchmarking

Use a build that preserves symbols while remaining optimized:

```shell
cmake --preset=x-android-aarch64 -B build \
-DBUILD_BENCHMARK=ON \
-DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

See  [Benchmark notes](../skills/llm-benchmark-workflow/references/bench-notes.md) for more information


Build the LLM framework for the target device with benchmarking and Streamline support enabled. These options ensure that Gator annotations are emitted and that function-level execution can be resolved in Streamline.


For SME-capable aarch64 targets, verify available architecture on your target device and set a supported CPU architecture which includes +sme
Note: KleidiAI is enabled by default but requires +i8mm

```shell
cmake -B build --preset=x-linux-aarch64 \ 
-DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+i8mm+sme \
-DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## 3) Run baseline measurements

The benchmark binary`llm-bench-cli` allows you to capture encode/decode rates and time-to-first-token alongside wall-clock time. Ensure the benchmark binary is built by configuring with `-DBUILD_BENCHMARK=ON` and follow the build/run steps in `README.md` (`To build an executable benchmark binary` and `llm benchmark`).

Example:

```shell

./build/bin/llm-bench-cli -m resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf -i 128 -o 64 -c 2048 -t 1 -n 3 -w 1
```

For estimate of wall-clock time run llm-bench-cli for 3 measured iterations (with 1 warmup), benchmarking encode/decode performance for the specified model and token counts, 
 /usr/bin/time -v reports the process’s wall time and resource usage.


```shell
  /usr/bin/time -v ./build/bin/llm-bench-cli \
    -m resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf \
    -i 128 -o 64 -c 2048 -t 1 -n 3 -w 1
```

Collect at least 3 runs and report the median.

## 4) Compare SME on/off

SME kernels can also be toggled at runtime:

```shell
GGML_KLEIDIAI_SME=1 ./build/bin/llm-bench-cli \
-m resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf \
-i 128 -o 64 -c 2048 -t 1 -n 3 -w 1
```

Disable to compare:

```shell
GGML_KLEIDIAI_SME=0 ./build/bin/llm-bench-cli \
-m resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf \
-i 128 -o 64 -c 2048 -t 1 -n 3 -w 1
```

Record the delta in latency and throughput.

## 5) Profile with Arm Streamline

- Arm Streamline Performance Analyzer
  Streamline Performance Analyzer is a profiling tool that captures and visualizes performance data from Arm-based systems to help identify bottlenecks across CPU, GPU, and memory.
  It provides interactive charts and detailed metrics so developers can analyze application behavior, gain system-level insights and optimize performance efficiently.

- Arm Performance Studio
  Performance Studio is a free suite of performance analysis tools for profiling, debugging, and optimizing applications on Arm-based CPUs and GPUs (especially Android and Linux devices).

To enable additional timeline annotations using Arm Streamline, configure the build with:

```shell

cmake --preset=x-android-aarch64 -B build \

  -DLLM_FRAMEWORK=llama.cpp \
  -DBUILD_BENCHMARK=ON \
  -DENABLE_STREAMLINE=ON \
  
cmake --build build
```

When enabled, CMake fetches Arm Gator annotation sources and adds markers around the LLM wrapper lifecycle and JNI control-path entry points.

1. Install Arm Performance Studio (https://developer.arm.com/Tools%20and%20Software/Arm%20Performance%20Studio#Downloads) on your host and ensure the target has the Streamline data capture service (gator/streamline agent) installed.
2. Build the library.
3. Launch Streamline, create a new capture, and select the target device.
4. Select metrics for CPU, cache, and memory bandwidth. Where available, enable SVE/SME-related counters.
5. Start capture, run the `llm-bench-cli` workload (or `llama-cli` for an interactive run), then stop capture.
6. Inspect hotspots, core utilization, and memory pressure.

Use the capture to identify whether the workload is compute-bound, memory-bound, or impacted by scheduling.

### Gator Setup

Push the `gatord` binary to the target device. The binary is included with Arm Performance Studio and can be found at:

```bash
/Arm_Performance_Studio_2025.6_linux_x86-64/
└─ Arm_Performance_Studio_2025.6/
   └─ streamline/bin/android/arm64/gatord
```

Start Gator with process synchronization enabled so that data collection begins when the benchmark process starts:

```bash
./gatord --allow-command --wait-process llm-bench-cli
```

When Gator is ready, the terminal displays:

```text
Streamline Data Recorder v9.7.2 (Build 34d7747)
Copyright (c) 2010-2025 Arm Limited. All rights reserved.

Gator ready
```

## 6) Trace with Perfetto (SME analysis)

Perfetto helps analyze scheduling, CPU frequency changes, and system events.

1. Install Perfetto on the target or ensure it is available in PATH.
2. Create a trace config that includes CPU scheduling and frequency events.
3. Start the trace, run `llm-bench-cli`, then stop the trace.
4. Open the trace in the Perfetto UI to inspect CPU timelines and task slices.


Use these traces to compare SME-enabled vs. SME-disabled runs. Look for changes in CPU residency and scheduling behavior.

For more information please see:
[Perfetto CPU Profiling Guide](https://perfetto.dev/docs/getting-started/cpu-profiling)


## 7) Reporting template

Capture the following for each run:

| Field | Example                           |
| --- |-----------------------------------|
| Platform | Linux aarch64 (cross-compiled)    |
| Build preset | `x-linux-aarch64`                 |
| Build type | `RelWithDebInfo`                  |
| Model | `Llama-3.2-1B-Instruct-Q4_0.gguf` |
| Prompt length | 128 input / 64 output             |
| Median latency | 2.1s                              |
| Peak RSS | 1.2 GB                            |
| Notes | SMT on                            |

*SMT = Simultaneous Multi-Threading