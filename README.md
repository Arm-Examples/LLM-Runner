<!--
    SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->


# LLM library

## Table of Contents

- [Overview](#overview)
- [Documentation](#documentation)
- [Quick start](#quick-start)
- [Supported Models](#supported-models)
- [Supported Platforms](#supported-platforms)
- [Framework specific configuration options](#framework-specific-configuration-options)
  - [llama cpp options](#llama-cpp-options)
  - [onnxruntime genai options](#onnxruntime-genai-options)
  - [mediapipe options](#mediapipe-options)
  - [mnn options](#mnn-options)
- [Shared libraries build parameter](#shared-libraries-build-parameter)
- [Known Issue with llama.cpp](#known-issue-with-llamacpp)
- [llama cpp model](#llama-cpp-model)
  - [llama cpp multimodal](#llama-cpp-multimodal)
- [onnxruntime genai model](#onnxruntime-genai-model)
- [mediapipe model](#mediapipe-model)
- [mnn model](#mnn-model)
  - [mnn multimodal](#mnn-multimodal)
- [To build an executable benchmark binary](#to-build-an-executable-benchmark-binary)
- [arm llm benchmark](#arm-llm-benchmark)
- [Troubleshooting](#troubleshooting)
- [Contributions](#contributions)
- [Trademarks](#trademarks)
- [License](#license)

---

## Overview

This repo is designed for building an
[Arm® KleidiAI™](https://www.arm.com/markets/artificial-intelligence/software/kleidi)
enabled LLM library using CMake build system.
Provides a single API (Java & C++) to various LLM frameworks
that Arm® KleidiAI™ kernels have been integrated into.
Currently, it supports [llama.cpp](https://github.com/ggml-org/llama.cpp),
[mediapipe](https://github.com/google-ai-edge/mediapipe),
[onnxruntime-genai](https://github.com/microsoft/onnxruntime-genai), and
[MNN](https://github.com/alibaba/MNN) backends.
The backend library (selected at CMake configuration stage) is wrapped by this project's thin
C++ layer that could be used directly for testing and evaluations.
However, JNI bindings are also provided for developers targeting Android™ based applications.

# Quick start

This guide covers the recommended build and run flows for supported platforms. For configuration options and model details, see `docs/build_and_config_guide.md`.

## Prerequisites

* A Linux®-based operating system is recommended (this repo is tested on Ubuntu® 22.04.4 LTS)
* An Android™ or Linux® device with an Arm® CPU is recommended as a deployment target, but this
  library can be built for any native machine.
* CMake 3.28 or above installed
* Python 3.9 or above installed, python is used to download test resources and models
* Android™ NDK (if building for Android™). Minimum version: 29.0.14206865 is recommended and can be downloaded
  from [here](https://developer.android.com/ndk/downloads).
* Building on macOS requires Xcode Command Line Tools, Android Studio installed and configured (NDK, CMake as above) and Clang (tested with 16.0.0)
* Bazelisk or Bazel 7.4.1 to build mediapipe backend
* Aarch64 GNU toolchain (version 14.1 or later) if cross-compiling from a Linux® based system which can be downloaded from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
* Java Development Kit required for building JNI wrapper library necessary to utilise this module in an Android/Java application.
* Create a [Hugging Face](https://huggingface.co) account and obtain a Hugging Face access token.

## Quick start

The project can be built and LLM tests exercised by simply running the following commands on supported platforms:

```shell
cmake --preset=native -B build 
cmake --build ./build
ctest --test-dir ./build
```

The commands above will use the default LLM framework (llama.cpp) and download a small number of LLM models. The tests exercise both vision and text queries. See [`LlmTest.cpp`](test/cpp/LlmTest.cpp) & [`LlmTestJNI.java`](test/java/com/arm/LlmTestJNI.java) for details.


**ctest --test-dir ./build** command above should produce results similar to those give below (timings may vary):

```shell
Internal ctest changing into directory: /home/user/llm/build
Test project /home/user/llm/build
    Start 1: llm-cpp-ctest
1/2 Test #1: llm-cpp-ctest ....................   Passed    4.16 sec
    Start 2: llama-jni-ctest
2/2 Test #2: llama-jni-ctest ..................   Passed    3.25 sec

100% tests passed, 0 tests failed out of 2
```


## Documentation

| Document                                             | Purpose |
|------------------------------------------------------| --- |
| [`docs/README.md`](docs/README.md)                   | Documentation index and update guidance. |
| [`docs/build_and_config_guide.md`](docs/build_and_config_guide.md)           | Build/run steps, platform matrix, and common build commands. |
| [`docs/architecture.md`](docs/architecture.md)       | Architecture overview, components, and execution flow. |
| [`docs/benchmarking.md`](docs/benchmarking.md)         | Benchmarking and profiling guidance. |
| [`docs/integration.md`](docs/integration.md)             | Configuration options and integration notes. |
| [`docs/troubleshooting.md`](docs/troubleshooting.md) | Common errors and fixes. |
| [`docs/contributing.md`](docs/contributing.md)       | Contribution process and SPDX guidance. |

## Repository Layout

| Source Folder                | Purpose                                                                                         |
|------------------------------|-------------------------------------------------------------------------------------------------|
| `src/cpp/`                   | Core C++ wrapper implementing the LLM-Runner abstraction layer and backend integration.         |
| `src/java/`                  | Java/JNI bindings.                                                                              |
| `scripts/py/`                | Python utilities for downloading models, test resources, and performing data preparation tasks. |
| `scripts/cmake/`             | Toolchains and CMake helper scripts for cross-compilation and platform configuration.           |
| `model_configuration_files/` | Model configuration files used by the build system and runtime.                                 |
| `resources_downloaded/`      | Default directory where models and example assets are downloaded.                               |
| `test/`                      | C++/Java unit tests  and supporting test resources.                                             |


## Supported Models

| Framework / Backend    | Supported Models                           | Licenses                                                                                                                                                                                                                                       |
|------------------------|--------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **llama.cpp**          | `phi-2`<br/>`qwen-2-VL`<br/>`llama-3.2-1B` | [mit](https://huggingface.co/microsoft/phi-2/blob/main/LICENSE)<br/> [apache-2.0](https://huggingface.co/Qwen/Qwen2-VL-2B-Instruct/blob/main/LICENSE)<br/> [Llama-3.2-1B](https://huggingface.co/meta-llama/Llama-3.2-1B/blob/main/LICENSE.txt) |
| **onnxruntime-genai**  | `phi4-mini-instruct`                       | [mit](https://huggingface.co/microsoft/Phi-4-mini-instruct/blob/main/LICENSE)                                                                                                                                                                  |
| **mediapipe**          | `gemma-2B`                                 | [Gemma](https://www.kaggle.com/models/google/gemma/license/consent)                                                                                                                                                                             |
| **mnn**                | `qwen-2.5-VL`<br/>`llama-3.2-1B`           | [apache-2.0](https://huggingface.co/Qwen/Qwen2.5-VL-3B-Instruct/blob/main/LICENSE)<br/> [Llama-3.2-1B](https://huggingface.co/meta-llama/Llama-3.2-1B/blob/main/LICENSE.txt) |


## Supported Platforms

The supported build platforms and cmake presets matrix is given below.
The cmake presets (aka build target) are given in the first column and build platform are given in the first row.
So for example native builds have been tested on Linux-x86_64, Linux-aarch64 & macOS-aarch64. While x-android-aarch64 (targets Android™ devices running on aarch64) builds are only tested on Linux-x86_64 & macOS-aarch64.

|  cmake-preset / Host Platform  | Linux-x86_64 | Linux-aarch64                      | macOS-aarch64 | Android™ |
|--------------------------------------|--------------|------------------------------------|---------------|---------|
| native                               | ✅            | ✅ *                              | ✅            | -      |
| x-android-aarch64                    | ✅            | -                                 | ✅            | -      |
| x-linux-aarch64                      | ✅ *          | ✅ †                              | -            | -      |


\* When targeting the Linux-aarch64 platform and the llama.cpp backend (using either native or x-linux-aarch64 presets) CPU_ARCH build flag must be specified. See the [CPU_ARCH table](#cpu-arch-table) for supported configuration.
† Use 'native' preset

Configuration option can be used with cmake presets.

For example KleidiAI acceleration can be disabled by setting USE_KLEIDIAI=OFF, e.g.
This is useful when testing the uplift in performance due to Arm CPU hardware acceleration.

```shell
cmake --preset=native -B build -DUSE_KLEIDIAI=OFF
cmake --build ./build
ctest --test-dir ./build
```

LLM_FRAMEWORK can be used to select the LLM framework, e.g.

```shell
cmake --preset=native -B build -DLLM_FRAMEWORK=onnxruntime-genai
cmake --build ./build
ctest --test-dir ./build
```

Details of configurable build options are given below:

Flag name | Default | Values                                                                                                   | Description                                                                                                                               |
|---|---|----------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------|
| LLM_FRAMEWORK | llama.cpp | llama.cpp / mediapipe / onnxruntime-genai / mnn                                                          | Specifies the backend framework to be used.                                                                                               |
| BUILD_DEBUG | OFF | ON/OFF                                                                                                   | If set to ON a debug build is configured.                                                                                                 |
| ENABLE_STREAMLINE | OFF | ON/OFF                                                                                                   | Enables Arm Streamline timeline annotations for analyzing LLM initialization, encode, decode, and control-path performance.               |
| BUILD_LLM_TESTING | ON | ON/OFF                                                                                                   | Builds the project's functional tests when ON.                                                                                            |
| BUILD_BENCHMARK | OFF | ON/OFF                                                                                                   | Builds the framework's benchmark binaries and arm-llm-bench-cli for the project when ON.                                                  |
| BUILD_JNI_LIB| ON | ON/OFF                                                                                                   | Builds the JNI bindings for the project.                                                                                                  |
| LOG_LEVEL | INFO/DEBUG | DEBUG, INFO, WARN &  ERROR                                                                               | For BUILD_DEBUG=OFF the default value is INFO. For BUILD_DEBUG=ON, the default value is DEBUG.                                            |
| USE_KLEIDIAI | ON | ON/OFF                                                                                                   | Build the project with KLEIDIAI CPU optimizations; if set to OFF, optimizations are turned off.                                           |
| CPU_ARCH | Not defined | Armv8.2_1, Armv8.2_2, Armv8.2_3, Armv8.2_4, Armv8.2_5, Armv8.6_1, Armv9.0_1_1, armv9.2_1_1, armv9.2_2_1 | Sets the target ISA architecture (AArch64) to ensure SVE is not enabled when LLM_FRAMEWORK=llama.cpp  (issue affects aarch64 only). |
| GGML_METAL | OFF         | ON/OFF                                                                                                   | macOS specific. Enables Apple Metal backend in ggml for GPU acceleration (Apple Silicon only).                                            |
| GGML_BLAS  | OFF         | ON/OFF                                                                                                   | macOS specific. Enables Accelerate/BLAS backend in ggml for CPU-optimized linear algebra kernels.                                         |

- `DOWNLOADS_LOCK_TIMEOUT`: A timeout value in seconds indicating how much time a lock should be tried for
  when downloading resources. This is a one-time download that CMake configuration will initiate unless it
  has been run by the user directly or another prior CMake configuration. The lock prevents multiple CMake
  configuration processes running in parallel downloading files to the same location.

### Framework specific configuration options

There are different conditional options for different frameworks.

#### llama cpp options

For `llama.cpp` as framework, these configuration parameters can be set:
- `LLAMA_SRC_DIR`: Source directory path that will be populated by CMake
  configuration.
- `LLAMA_GIT_URL`: Git URL to clone the sources from.
- `LLAMA_GIT_SHA`: Git SHA for checkout.
- `LLAMA_BUILD_COMMON`: Build llama's dependency Common, <b>enabled by default.</b>
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

> **NOTE**: This repository has been tested with `onnxruntime` version `v1.24.2` and
`onnxruntime-genai` version `v0.12.0`.

#### mediapipe options

For customising mediapipe framework , following parameters can be used:

- `MEDIAPIPE_SRC_DIR`: Source directory path that will be populated by CMake
  configuration.
- `MEDIAPIPE_GIT_URL`: Git URL to clone the sources from.
- `MEDIAPIPE_GIT_TAG`: Git SHA for checkout

Building mediapipe for aarch64 in x86_64 linux based requires downloading Aarch64 GNU toolchain from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads), following configuration flags need to provided for building
- `BASE_PATH`: Provides the top level directory of aarch64 GNU toolchain, if not provided the build script will download the latest ARM GNU toolchain for cross-compilation.
> **NOTE**: Support for mediapipe is experimental and current focus is to support Android™ platform. Please note that latest ARM GNU Toolchain version(14.3) may depend on libraries present in Ubuntu® 24.04.4 LTS when cross-compiled.\
> Support for macOS® and Windows is not added in this release.

#### mnn options

For customising MNN framework , following parameters can be used:

- `MNN_SRC_DIR`: Source directory path that will be populated by CMake
  configuration.
- `MNN_GIT_URL`: Git URL to clone the sources from.
- `MNN_GIT_TAG`: Git SHA for checkout

> **NOTE**: This repository has been tested with `MNN` version `v3.3.0`.

> **KleidiAI™ NOTE**: :
Although MNN can be built with USE_KLEIDIAI defined, the current MNN implementation does not fully enable KleidiAI™ optimizations at runtime.
This limitation is due to the current MNN runtime initialization logic and will be resolved once full support is implemented upstream in MNN.

### Shared libraries build parameter

When targeting the llama.cpp LLM backend and Android (--preset=x-android-aarch64),  BUILD_SHARED_LIBS=ON is automatically configured. This ensures the build generates shared libraries, allowing the optimal hardware accelerated libraries to be loaded for the particular device at runtime.

## Known Issue with llama.cpp

Currently there are issues with a specific architecture (SVE) integration in llama.cpp backend on aarch64. To ensure this feature is not enabled we enforce using one of our provided `CPU_ARCH` flag presets
that ensure compiler flags do not enable SVE at build time.
The table below gives the mapping of our preset CPU_ARCH flags to some common CPU feature flag sets.
Other permutations are also supported and can be tailored accordingly. If you intend to use specific features you must ensure your specific CPU implements them e.g. i8mm  as this was
optional in v8.2 for example. Compilers also need to support any chosen features.

<a id="cpu-arch-table"></a>

| CPU_ARCH     | C/C++ compiler flags                         |
|--------------|----------------------------------------------|
| Armv8.2_1    | -march=armv8.2-a+dotprod                     |
| Armv8.2_2    | -march=armv8.2-a+dotprod+fp16                |
| Armv8.2_3    | -march=armv8.2-a+dotprod+fp16+sve            |
| Armv8.2_4    | -march=armv8.2-a+dotprod+i8mm                |
| Armv8.2_5    | -march=armv8.2-a+dotprod+i8mm+sve+sme        |
| Armv8.6_1    | -march=armv8.6-a+dotprod+fp16+i8mm           |
| Armv9.0_1_1  | -march=armv8.6-a+dotprod+fp16+i8mm+nosve     |
| *armv9.2_1_1 | -march=armv9.2-a+dotprod+fp16+nosve+i8mm+sme |
| *armv9.2_2_1 | -march=armv9.2-a+dotprod+fp16+nosve+i8mm+sme |

* Note: Different capitalisation for v9.2 presets.


> **NOTE**: If you need specific version of Java set the path in `JAVA_HOME` environment variable.
> ```shell
> export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
> ```
> Failure to locate "jni.h" occurs if compatible JDK is not on the system path.
> If you want to experiment with the repository without JNI libs, turn the `BUILD_JNI_LIB` option off by
> configuring with `-DBUILD_JNI_LIB=OFF`.
> On first LLM initialization, the module also emits a build metadata line to CLI logs and Android logcat
> containing the selected backend, pinned backend dependency revisions, module version/git SHA, and build timestamp.


### llama cpp model

This project uses the **phi-2 model** as its default network for `llama.cpp` framework.
The model is distributed using the **Q4_0 quantization format**, which is highly recommended as it
delivers effective inference times by striking a balance between computational efficiency and model performance.

- You can access the model from [Hugging Face](https://huggingface.co/ggml-org/models/blob/main/phi-2/ggml-model-q4_0.gguf).
- The default model configuration is declared in the [`requirements.json`](scripts/py/requirements.json) file.

However, any model supported by the backend library could be used.

> **NOTE**: Currently only Q4_0 models are accelerated by Arm® KleidiAI™ kernels in `llama.cpp`.


#### llama cpp multimodal

The `llama.cpp` backend **also supports multimodal (image + text)** inference in this project.

**What you need**
- A compatible **text model** (GGUF).
- A matching **vision projection (mmproj) file** (GGUF) for your chosen text model

**How to enable**
Use these fields in your configuration file:

- `llmModelName` — text model (GGUF)
- `llmMmProjModelName` — vision projection (GGUF) for multimodal
- `isvision` — set `"true"` to enable multimodal

If `"isVision"` is `true`, a valid `llmMmProjModelName` is required; omitting `"image"` runs the backend in **text-only** mode.

You can find an example of multimodal settings in [`llamaVisionConfig-qwen2-vl-2B.json`](model_configuration_files/llamaVisionConfig-qwen2-vl-2B.json).

### onnxruntime genai model

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

### mediapipe model

To use the **Gemma 2B** model, add your [Hugging Face](https://huggingface.co) access token to the build environment after accepting the [*Gemma license*](https://www.kaggle.com/models/google/gemma/license/consent) .
```shell
export HF_TOKEN=<your hugging-face access token>
```

or

Append the following lines to your ~/.netrc file:
```text
machine huggingface.co
  login <your-username-or-email>
  password <your-huggingface-access-token>
```
Ensure the .netrc file is secured with the correct permissions.
Alternatively, you can quantize other models listed in [conversion colab](https://colab.research.google.com/github/googlesamples/mediapipe/blob/main/examples/llm_inference/conversion/llm_conversion.ipynb) from [Hugging Face](https://huggingface.co) to TensorFlow Lite™ (.tflite) format. Copy the resulting 4-bit models to `resources_downloaded/models/mediapipe`.
It is recommended to use *mediapipe python package version 0.10.15* for stable conversion to 4-bit models.

### mnn model

This project uses the **Llama 3.2 1B model** as its default network for the MNN framework.
The model is distributed using the **4-bit quantization** format, which is highly recommended as it delivers efficient inference performance while maintaining strong text generation quality on Arm® CPUs.

- You can access the text model from [Hugging Face](https://huggingface.co/taobao-mnn/Llama-3.2-1B-Instruct-MNN)
- The model configuration is declared in the [`requirements.json`](scripts/py/requirements.json)

However, any model supported by the MNN backend library can be used.

To use an MNN model with this framework, the following files are required:
- `config.json`: Model configuration file
- `llm.mnn`: Main MNN model file
- `llm.mnn.json`: Model metadata file generated by the MNN conversion process
- `llm.mnn.weight`: Model weight file (used when weights are stored separately)
- `llm_config.json`: Model-specific configuration file
- `tokenizer.txt` : Tokenizer definition file
- `embeddings_bf16.bin` : (optional) Used by some models that store embeddings separately. If this file exists, download it; otherwise, embeddings are already included in the main weights.

These files are essential for loading and running MNN models effectively.

#### mnn multimodal

The `MNN` backend **also supports multimodal (image + text)** inference in this project.

- You can access the vision model from [Hugging Face](https://huggingface.co/taobao-mnn/Qwen2.5-VL-3B-Instruct-MNN)

**What you need**
- `visual.mnn`: Vision model metadata file generated by the MNN conversion process
- `visual.mnn.weight`: Vision model weight file (used when weights are stored separately)

> **NOTE**: The MNN backend determines whether multimodal mode is active from the `is_visual` field inside the model’s `llm_config.json`.

You can find an example multimodal configuration in [mnnVisionConfig-qwen2.5-3B.json](model_configuration_files/mnnVisionConfig-qwen2.5-3B.json)

## To build an executable benchmark binary

To build a standalone benchmark binary add the configuration option `-DBUILD_BENCHMARK=ON`
to any of the build commands above. For example:

On Aarch-64
```shell
cmake -B build --preset=native -DCPU_ARCH=Armv8.2_4 -DBUILD_BENCHMARK=ON
cmake --build ./build
```
The benchmark summary and JSON output report `model_size` as a formatted value e.g. `1.23 GB`.
The size is derived from the total configured model package for the benchmarked model path.
If the configured path is a directory, the size is computed recursively.


## arm llm benchmark

The Arm LLM Benchmark tool (arm-llm-bench-cli) is a framework-agnostic, standalone executable designed to measure both prompt-processing and token-generation performance across all supported LLM backends.

**Supported Frameworks**
- `llama.cpp`
- `onnxruntime-genai`
- `MNN`
- `mediapipe`

Instead of writing your own prompts or relying on framework-specific benchmarking tools, `arm-llm-bench-cli` provides a unified benchmarking pipeline. It automatically detects the backend specified in the LLM configuration file and benchmarks it consistently. The tool repeatedly runs the LLM prompt-processing and token-generation  operations and reports timing and throughput metrics in a standardized format.

> **NOTE**: To build `arm-llm-bench-cli`, ensure the benchmarking flag is set in CMake by setting `-DBUILD_BENCHMARK=ON`.

**Measures**

- `Encode time and encode tokens/s`
- `Decode time and decode tokens/s`
- `Time-to-first-token (TTFT)`
- `Total latency per iteration`
- `Supports warm-up iterations (ignored in statistics)`

**Usage**
```
./build/bin/arm-llm-bench-cli \
    --model     <model_path>          | -m <model_path> \
    --input     <tokens>              | -i <tokens> \
    --output    <tokens>              | -o <tokens> \
    --threads   <num_threads>         | -t <num_threads> \
    --iterations <num_iterations>     | -n <num_iterations> \
    [ --context <tokens>              | -c <tokens> ] \
    [ --json-output <path>            | -J <path> ] \
    [ --warmup <warmup_iterations>    | -w <warmup_iterations> ]
```

> **NOTE**: On-device execution requires that `arm-llm-bench-cli` and its backend shared libraries reside in the same directory. Builds using `GGML_OPENMP=ON` additionally require `libomp.so` to be placed in that directory as well.

**Example**
```
./build/bin/arm-llm-bench-cli \
    -m ./resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf \
    -i 128 \
    -o 64 \
    -c 2048 \
    -t 4 \
    -n 3 \
    -w 1 \
    -J /path/to/result.json

Terminal Output:

INFO : Running 1 warmup iteration(s) (results ignored)...

=== ARM LLM Benchmark ===

Parameters:
  model_path         : ./resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf
  model_size         : 0.77 GB
  num_input_tokens   : 128
  num_output_tokens  : 64
  context_size       : 2048
  num_threads        : 4
  num_iterations     : 3
  num_warmup         : 1


======= Results =========

| Framework          | Threads | Test   | Performance                |
| ------------------ | ------- | ------ | -------------------------- |
| llama.cpp          | 5       | pp128  |   204.149 ±  4.316 (t/s)   |
| llama.cpp          | 5       | tg64   |    48.029 ±  0.080 (t/s)   |
| llama.cpp          | 5       | TTFT   |   648.401 ± 13.798 (ms)    |
| llama.cpp          | 5       | Total  |  1959.827 ± 14.433 (ms)    |

JSON output written to: /path/to/result.json
```

## Troubleshooting

For a list of common errors and their fixes, see [`docs/troubleshooting.md`](docs/troubleshooting.md).

## Contributions

The LLM-Runner welcomes contributions. For more details on contributing to the repo please see the [contributors guide](docs/contributing.md#contributions).

## Trademarks

* Arm® and KleidiAI™ are registered trademarks or trademarks of Arm® Limited (or its subsidiaries) in the US and/or
  elsewhere.
* Android™ and TensorFlow Lite™ are trademarks of Google LLC.
* macOS® is a trademark of Apple Inc.

## License

This project is distributed under the software licenses in [LICENSES](LICENSES) directory.
The licenses of supported models can be seen in [Supported Models section](#supported-models).
