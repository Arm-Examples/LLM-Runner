<!--
    SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->


# LLM library

<!-- TOC -->
* [LLM library](#llm-library)
  * [Prerequisites](#prerequisites)
  * [Configuration options](#configuration-options)
    * [Conditional options](#conditional-options)
      * [llama cpp options](#llama-cpp-options)
      * [onnxruntime genai options](#onnxruntime-genai-options)
  * [Quick start](#quick-start)
    * [Neural network](#neural-network)
      * [llama cpp model](#llama-cpp-model)
      * [onnxruntime genai model](#onnxruntime-genai-model)
    * [To build for Android](#to-build-for-android)
    * [To build for Linux](#to-build-for-linux)
      * [Generic aarch64 target](#generic-aarch64-target)
      * [Aarch64 target with SME](#aarch64-target-with-sme)
      * [Native host build](#native-host-build)
  * [Building and running tests](#building-and-running-tests)
  * [To build an executable](#to-build-an-executable)
    * [llama cpp](#llama-cpp)
    * [onnxruntime genai](#onnxruntime-genai)
  * [Trademarks](#trademarks)
  * [License](#license)
<!-- TOC -->

This repo is designed for building an
[Arm® KleidiAI™](https://www.arm.com/markets/artificial-intelligence/software/kleidi)
enabled LLM library using CMake build system. It intends to provide an abstraction for different Machine Learning
frameworks/backends that Arm® KleidiAI™ kernels have been integrated into.
Currently, it supports [llama.cpp](https://github.com/ggml-org/llama.cpp) and
[onnxruntime-genai](https://github.com/microsoft/onnxruntime-genai) backends but we intend to add
support for other backends, such as [mediapipe](https://github.com/google-ai-edge/mediapipe), soon.

The backend library (selected at CMake configuration stage) is wrapped by this project's thin C++ layer that could be used
directly for testing and evaluations. However, JNI bindings are also provided for developers targeting Android™ based
applications.

## Prerequisites

* A Linux®-based operating system is recommended (this repo is tested on Ubuntu® 22.04.4 LTS)
* An Android™ or Linux® device with an Arm® CPU is recommended as a deployment target, but this
  library can be built for any native machine.
* CMake 3.27 or above installed
* Python 3.9 or above installed, python is used to download test resources and models
* Android™ NDK (if building for Android™). Minimum version: r27 is recommended and can be downloaded
  from [here](https://developer.android.com/ndk/downloads)
* Aarch64 GNU toolchain (version 14.1 or later) if cross-compiling from a Linux® based system which can be downloaded from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
* Java Development Kit required for building JNI wrapper library necessary to utilise this module in an Android/Java application.

## Configuration options

The project is designed to download the required software sources based on user
provided configuration options. CMake presets are available to use and set the following variables:

- `LLM_FRAMEWORK`: Currently supports `llama.cpp` (default framework) and `onnxruntime-genai`.
- `BUILD_JNI_LIB`: Build the JNI shared library that other projects can consume, <b>enabled by default.</b>
- `BUILD_UNIT_TESTS`: Build C++ unit tests and add them to CTest, JNI tests will also be built, <b>enabled by default.</b>
- `BUILD_EXECUTABLE`: Build standalone applications, <b>disabled by default.</b>

> **NOTE**: If you need specific version of Java set the path in `JAVA_HOME` environment variable.
> ```shell
> export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
> ```
> Failure to locate "jni.h" occurs if compatible JDK is not on the system path.
> If you want to experiment with the repository without JNI libs, turn the `BUILD_JNI_LIB` option off by
> configuring with `-DBUILD_JNI_LIB=OFF`.

- `DOWNLOADS_LOCK_TIMEOUT`: A timeout value in seconds indicating how much time a lock should be tried for
  when downloading resources. This is a one-time download that CMake configuration will initiate unless it
  has been run by the user directly or another prior CMake configuration. The lock prevents multiple CMake
  configuration processes running in parallel downloading files to the same location.

### Conditional options

There are different conditional options for different frameworks.

#### llama cpp options

For `llama.cpp` as framework, these configuration parameters can be set:
- `LLAMA_SRC_DIR`: Source directory path that will be populated by CMake
  configuration.
- `LLAMA_GIT_URL`: Git URL to clone the sources from.
- `LLAMA_GIT_SHA`: Git SHA for checkout.
- `LLAMA_BUILD_COMMON`: Build llama's dependency Common, <b>enabled by default.</b>
- `BUILD_SHARED_LIBS`: Build shared instead of static dependency libraries, specifically - ggml and common, <b>disabled by default.</b>
- `LLAMA_CURL`: Enable HTTP transport via libcurl for remote models or features requiring network communication, <b>disabled by default.</b>

#### onnxruntime genai options

When using `onnxruntime-genai`, the `onnxruntime` dependency will be built from source. To customize
the versions of both `onnxruntime` and `onnxruntime-genai`, the following configuration parameters
can be used:

onnxruntime:
- `ONNXRUNTIME_SRC_DIR`: Source directory path that will be populated by CMake
  configuration.
- `ONNXRUNTIME_GIT_URL`: Git URL to clone the sources from.
- `ONNXRUNTIME_GIT_TAG`: Git SHA for checkout.

onnxruntime-genai:
- `ONNXRT_GENAI_SRC_DIR`: Source directory path that will be populated by CMake
  configuration.
- `ONNXRT_GENAI_GIT_URL`: Git URL to clone the sources from.
- `ONNXRT_GENAI_GIT_TAG`: Git SHA for checkout.

> **NOTE**: This repository has been tested with `onnxruntime` version `v1.22.0` and
`onnxruntime-genai` version `v0.8.2`.

## Quick start

By default, the JNI builds are enabled, and Arm® KleidiAI™ kernels are enabled on arm64/aarch64.
To disable these, configure with: `-DUSE_KLEIDIAI=OFF`.

### Neural network

There are different default model for different frameworks.

#### llama cpp model

This project uses the **phi-2 model** as its default network for `llama.cpp` framework.
The model is distributed using the **Q4_0 quantization format**, which is highly recommended as it
delivers effective inference times by striking a balance between computational efficiency and model performance.

- You can access the model from [Hugging Face](https://huggingface.co/ggml-org/models/blob/main/phi-2/ggml-model-q4_0.gguf).
- The default model configuration is declared in the [`requirements.json`](scripts/py/requirements.json) file.

However, any model supported by the backend library could be used.

> **NOTE**: Currently only Q4_0 models are accelerated by Arm® KleidiAI™ kernels in `llama.cpp`.

#### onnxruntime genai model

This project uses the **Phi-4-mini-instruct-onnx** as its default network for `onnxruntime-genai` framework.
The model is distributed using **int4 quantization format** with the **block size: 32**, which is highly recommended as it
delivers effective inference times by striking a balance between computational efficiency and model performance.

- You can access the model from [Hugging Face](https://huggingface.co/microsoft/Phi-4-mini-instruct-onnx/tree/main/cpu_and_mobile/cpu-int4-rtn-block-32-acc-level-4).
- The default model configuration is declared in the [`requirements.json`](scripts/py/requirements.json) file.

However, any model supported by the backend library could be used.

To use an ONNX model with this framework, the following files are required:
- `genai_config.json`: Configuration file
- `model_name.onnx`: ONNX model
- `model_name.onnx.data`: ONNX model data
- `tokenizer.json`: Tokenizer file
- `tokenizer_config.json`: Tokenizer config file

These files are essential for loading and running ONNX models effectively.

> **NOTE**: Currently only int4 and block size 32 models are accelerated by Arm® KleidiAI™ kernels in `onnxruntime-genai`.

### To build for Android
For Android™ build, ensure the `NDK_PATH` is set to installed Android™ NDK, specify Android™ ABI and platform if required or use a default preset e.g. android-arm64-release-kleidi-on-v82a-dotprod-i8mm
```shell
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-33 \
    -DCMAKE_C_FLAGS=-march=armv8.2-a+i8mm+dotprod \
    -DCMAKE_CXX_FLAGS=-march=armv8.2-a+i8mm+dotprod

cmake --build ./build
```

### To build for Linux

Building for Linux targets, with `llama.cpp` backend, `GGML_CPU_ARM_ARCH` can be set to provide the architecture flags.

#### Generic aarch64 target

As an example, for a target with `FEAT_DOTPROD` and `FEAT_I8MM` available, the configuration command might be:

```shell
cmake -B build \
    --preset=elinux-aarch64-release-with-tests \
    -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+i8mm \
    -DBUILD_EXECUTABLE=ON

cmake --build ./build
```

#### Aarch64 target with SME

To build for aarch64 Linux system with [Scalable Matrix Extensions](https://developer.arm.com/documentation/109246/0100/SME-Overview/SME-and-SME2), for `llama.cpp` ensure `GGML_CPU_ARM_ARCH` is set with needed feature flags as below:

```shell
cmake -B build \
    --preset=elinux-aarch64-release-with-tests \
    -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+i8mm+sve+sme \
    -DBUILD_EXECUTABLE=ON

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

#### Native host build

```shell
cmake -B build --preset=native-release-with-tests
cmake --build ./build
```

## Building and running tests

To build and test for native host machine:

```shell
cmake -B build --preset=native-release-with-tests
cmake --build ./build
ctest --test-dir ./build
```

> **NOTE**: For consistent and reliable test results, avoid using the `--parallel` option when running tests.

This should produce something like:
```shell
Internal ctest changing into directory: /home/user/llm/build
Test project /home/user/llm/build
    Start 1: llm-cpp-ctest
1/2 Test #1: llm-cpp-ctest ....................   Passed    4.16 sec
    Start 2: llama-jni-ctest
2/2 Test #2: llama-jni-ctest ..................   Passed    3.25 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   7.41 sec
```

Even when cross-compiling, the test binaries can be copied to the target system and executed.

## To build an executable

To build a standalone application add the configuration option `-DBUILD_EXECUTABLE=ON` to any of the build
commands above. For example:

On Aarch-64
```shell
cmake -B build \
    --preset=elinux-aarch64-release-with-tests \
    -DCMAKE_C_FLAGS=-march=armv8.2-a+dotprod+i8mm \
    -DCMAKE_CXX_FLAGS=-march=armv8.2-a+dotprod+i8mm \
    -DBUILD_EXECUTABLE=ON
cmake --build ./build


Or on x86 (No Kleidi Acceleration)

```shell
cmake -B build \
    --preset=native-release-with-tests \
    -DBUILD_EXECUTABLE=ON
cmake --build ./build
```

### llama cpp

You can run either executable from command line and add your prompt for example the following:
```
./build/bin/llama-cli -m resources_downloaded/models/llama.cpp/model.gguf --prompt "What is the capital of France"
```
More information can be found at `llama.cpp/examples/main/README.md` on how this executable can be run.

### onnxruntime genai

You can run model_benchmark executable from command line:
```
./build/bin/model_benchmark -i resources_downloaded/models/onnxruntime-genai
```
More information can be found at `onnxruntime-genai/benchmark/c/readme.md` on how this executable can be run.

## Trademarks

* Arm® and KleidiAI™ are registered trademarks or trademarks of Arm® Limited (or its subsidiaries) in the US and/or
  elsewhere.
* Android™ is a trademark of Google LLC.

## License

This project is distributed under the software licenses in [LICENSES](LICENSES) directory.
