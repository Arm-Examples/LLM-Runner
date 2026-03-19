<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
SPDX-License-Identifier: Apache-2.0
-->

## Build and configuration

This file outlines more granular build configuration options. 
Start with the [Quick start](../README.md#quick-start) section of the root README.

## Cross Compilation for Android

Cross compilation is also supported allowing the project to build binaries targeted to an OS/CPU architecture different from the host/build machine. For example it is possible to build the project on a Linux x86_64 platform and build binaries for Android™:

```shell
export NDK_PATH=/path/to/android-ndk
cmake --preset=x-android-aarch64  -B build 
cmake --build ./build
```

However, the binaries would need to be uploaded to an Android™ device to exercise the tests.
See the section below for additional cross-compilation options.

### Aarch64 target

To build for aarch64 Linux system

```shell
cmake -B build --preset=native -DCPU_ARCH=Armv8.2_4
cmake --build ./build
```

Once built, a standalone application can be executed to get performance.

If `FEAT_SME` is available on deployment target, environment variable `GGML_KLEIDIAI_SME` can be used to
toggle the use of SME kernels during execution for `llama.cpp`. For example:

```shell
GGML_KLEIDIAI_SME=1 ./build/bin/llama-cli -m resources_downloaded/models/llama.cpp/model.gguf -t 1 -p "What is a car?"
```

To run without invoking SME kernels, set `GGML_KLEIDIAI_SME=0` during execution:

```shell
GGML_KLEIDIAI_SME=0 ./build/bin/llama-cli -m resources_downloaded/models/llama.cpp/model.gguf -t 1 -p "What is a car?"
```

> **NOTE**: In some cases, it may be desirable to build a statically linked executable. For llama.cpp backend
> this can be done by adding these configuration parameters to the CMake command for Clang or GNU toolchains:
> ```shell
>    -DCMAKE_EXE_LINKER_FLAGS="-static"   \
>    -DGGML_OPENMP=OFF
> ```

### To Build for macOS

To build for the CPU backend on macOS®, you can use the native CMake toolchain.

```shell
cmake -B build --preset=native
cmake --build ./build
```
> **NOTE**: If you need specific version of Java set the path in `JAVA_HOME` environment variable.
> ```shell
> export JAVA_HOME=$(/usr/libexec/java_home)
> ```

Once built, a standalone application can be executed to get performance.

If `FEAT_SME` is available on deployment target, environment variable `GGML_KLEIDIAI_SME` can be used to
toggle the use of SME kernels during execution for `llama.cpp`. For example:

```shell
GGML_KLEIDIAI_SME=1 ./build/bin/llama-cli -m resources_downloaded/models/llama.cpp/model.gguf -t 1 -p "What is a car?"
```

To run without invoking SME kernels, set `GGML_KLEIDIAI_SME=0` during execution:

```shell
GGML_KLEIDIAI_SME=0 ./build/bin/llama-cli -m resources_downloaded/models/llama.cpp/model.gguf -t 1 -p "What is a car?"
```

### llama cpp

You can run either executable from command line and add your prompt for example the following:
```
./build/bin/llama-cli -m resources_downloaded/models/llama.cpp/phi-2/phi2_Q4_model.gguf --prompt "What is the capital of France"
```
More information can be found at `llama.cpp/examples/main/README.md` on how this executable can be run.

### onnxruntime genai

You can run model_benchmark executable from command line:
```
./build/bin/model_benchmark -i resources_downloaded/models/onnxruntime-genai/phi-4-mini/
```
More information can be found at `onnxruntime-genai/benchmark/c/readme.md` on how this executable can be run.

### mnn

You can run llm_bench executable from command line:
```
./build/bin/llm_bench -m resources_downloaded/models/mnn/llama-3.2-1b/config.json -t 4 -p 128 -n 64
```
