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
      * [mediapipe options](#mediapipe-options)
  * [Quick start](#quick-start)
    * [Supported Models](#supported-models)
      * [llama cpp model](#llama-cpp-model)
        * [multimodal](#multimodal)
      * [onnxruntime genai model](#onnxruntime-genai-model)
      * [mediapipe model](#mediapipe-model)
    * [Native host build](#native-host-build)
    * [To build for Android](#to-build-for-android)
    * [To build for Linux](#to-build-for-linux)
      * [Generic aarch64 target](#generic-aarch64-target)
      * [Aarch64 target with SME](#aarch64-target-with-sme)
    * [To Build for macOS](#to-build-for-macos)
  * [Building and running tests](#building-and-running-tests)
  * [To build an executable benchmark binary](#to-build-an-executable-benchmark-binary)
    * [llama cpp](#llama-cpp)
    * [onnxruntime genai](#onnxruntime-genai)
  * [Trademarks](#trademarks)
  * [License](#license)
<!-- TOC -->

This repo is designed for building an
[Arm® KleidiAI™](https://www.arm.com/markets/artificial-intelligence/software/kleidi)
enabled LLM library using CMake build system. It intends to provide an abstraction for different Machine Learning
frameworks/backends that Arm® KleidiAI™ kernels have been integrated into.
Currently, it supports [llama.cpp](https://github.com/ggml-org/llama.cpp) , [mediapipe](https://github.com/google-ai-edge/mediapipe) and
[onnxruntime-genai](https://github.com/microsoft/onnxruntime-genai) backends .
The backend library (selected at CMake configuration stage) is wrapped by this project's thin C++ layer that could be used
directly for testing and evaluations. However, JNI bindings are also provided for developers targeting Android™ based
applications.

## Prerequisites

* A Linux®-based operating system is recommended (this repo is tested on Ubuntu® 22.04.4 LTS)
* An Android™ or Linux® device with an Arm® CPU is recommended as a deployment target, but this
  library can be built for any native machine.
* CMake 3.28 or above installed
* Python 3.9 or above installed, python is used to download test resources and models
* Android™ NDK (if building for Android™). Minimum version: r27 is recommended and can be downloaded
  from [here](https://developer.android.com/ndk/downloads)
* Bazelisk or Bazel 7.4.1 to build mediapipe backend
* Aarch64 GNU toolchain (version 14.1 or later) if cross-compiling from a Linux® based system which can be downloaded from [here](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
* Java Development Kit required for building JNI wrapper library necessary to utilise this module in an Android/Java application.
* Create a [Hugging Face](https://huggingface.co) account and obtain a Hugging Face access token.

## Configuration options

The project is designed to download the required software sources based on user
provided configuration options. CMake presets are available to use and set the following variables:

- `LLM_FRAMEWORK`: Currently supports `llama.cpp` (default framework) and `onnxruntime-genai`.
- `BUILD_JNI_LIB`: Build the JNI shared library that other projects can consume, <b>enabled by default.</b>
- `BUILD_UNIT_TESTS`: Build C++ unit tests and add them to CTest, JNI tests will also be built, <b>enabled by default.</b>
- `BUILD_BENCHMARK`: Build benchmark binary, <b>enabled by default.</b>

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
- `LLM_LOG_LEVEL` Choose appropriate logging level ("ERROR","WARN","INFO","DEBUG") with this flag, if  LLM_LOG_LEVEL is not provided, it will be inferred from the CMAKE-BUILD-TYPE.

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

> **NOTE**: This repository has been tested with `onnxruntime` version `v1.22.2` and
`onnxruntime-genai` version `v0.9.0`.

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

## Quick start

By default, the JNI builds are enabled, and Arm® KleidiAI™ kernels are enabled on arm64/aarch64.
To disable these, configure with: `-DUSE_KLEIDIAI=OFF`.

### Supported Models

| Framework / Backend    | Supported Models                           | Licenses                                                                                                                                                                                                                                       |
|------------------------|--------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **llama.cpp**          | `phi-2`<br/>`qwen-2-VL`<br/>`llama-3.2-1B` | [mit](https://huggingface.co/microsoft/phi-2/blob/main/LICENSE)<br/> [apache-2.0](https://huggingface.co/Qwen/Qwen2-VL-2B-Instruct/blob/main/LICENSE)<br/> [Llama-3.2-1B](https://huggingface.co/meta-llama/Llama-3.2-1B/blob/main/LICENSE.txt) |
| **onnxruntime-genai**  | `phi4-mini-instruct`                       | [mit](https://huggingface.co/microsoft/Phi-4-mini-instruct/blob/main/LICENSE)                                                                                                                                                                  |
| **mediapipe**          | `gemma-2B`                                 | [Gemma](https://www.kaggle.com/models/google/gemma/license/consent)                                                                                                                                                                             |


#### llama cpp model

This project uses the **phi-2 model** as its default network for `llama.cpp` framework.
The model is distributed using the **Q4_0 quantization format**, which is highly recommended as it
delivers effective inference times by striking a balance between computational efficiency and model performance.

- You can access the model from [Hugging Face](https://huggingface.co/ggml-org/models/blob/main/phi-2/ggml-model-q4_0.gguf).
- The default model configuration is declared in the [`requirements.json`](scripts/py/requirements.json) file.

However, any model supported by the backend library could be used.

> **NOTE**: Currently only Q4_0 models are accelerated by Arm® KleidiAI™ kernels in `llama.cpp`.

##### multimodal

The `llama.cpp` backend **also supports multimodal (image + text)** inference in this project.

**What you need**
- A compatible **text model** (GGUF).
- A matching **vision projection (mmproj) file** (GGUF) for your chosen text model

**How to enable**
Use these fields in your configuration file:

- `llmModelName` — text model (GGUF)
- `llmMmProjModelName` — vision projection (GGUF) for multimodal
- `inputModalities` — include `"image"` to enable multimodal

If `"image"` is included in `inputModalities`, a valid `llmMmProjModelName` is required; omitting `"image"` runs the backend in **text-only** mode.

You can find an example of multimodal settings in [`llamaVisionConfig-qwen2-vl-2B.json`](model_configuration_files/llamaVisionConfig-qwen2-vl-2B.json).

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

#### mediapipe model

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

### Native host build

```shell
cmake -B build --preset=native-release-with-tests
cmake --build ./build
```
### To build for Android
For Android™ build, ensure the `NDK_PATH` is set to installed Android™ NDK, specify Android™ ABI and platform if required or use a default preset e.g. android-arm64-release-kleidi-on-v82a-dotprod-i8mm. 
```shell
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-33 \
    -DBUILD_SHARED_LIBS=ON

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
    -DBUILD_BENCHMARK=ON

cmake --build ./build
```

#### Aarch64 target with SME

To build for aarch64 Linux system with [Scalable Matrix Extensions](https://developer.arm.com/documentation/109246/0100/SME-Overview/SME-and-SME2), for `llama.cpp` ensure `GGML_CPU_ARM_ARCH` is set with needed feature flags as below:

```shell
cmake -B build \
    --preset=elinux-aarch64-release-with-tests \
    -DGGML_CPU_ARM_ARCH=armv8.2-a+dotprod+i8mm+sve+sme \
    -DBUILD_BENCHMARK=ON

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

> **NOTE**: Currently it is recommended for aarch64 Linux builds that the GGML_CPU_ARM_ARCH flag be set for the target system. 
> Not setting the flag may result in build issues and / or runtime errors.

### To Build for macOS

To build for the CPU backend on macOS®, you can use the native CMake toolchain.
However, additional flags are required to explicitly disable the default backends:
```shell
cmake -B build --preset=native-release-with-tests \
    -DGGML_METAL=OFF \
    -DGGML_BLAS=OFF
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

## To build an executable benchmark binary

To build a standalone benchmark binary add the configuration option `-DBUILD_BENCHMARK=ON` to any of the build
commands above. For example:

On Aarch-64
```shell
cmake -B build \
    --preset=elinux-aarch64-release-with-tests \
    -DCMAKE_C_FLAGS=-march=armv8.2-a+dotprod+i8mm \
    -DCMAKE_CXX_FLAGS=-march=armv8.2-a+dotprod+i8mm \
    -DBUILD_BENCHMARK=ON
cmake --build ./build
```

Or on x86 (No Kleidi Acceleration)
```shell
cmake -B build \
    --preset=native-release-with-tests \
    -DBUILD_BENCHMARK=ON
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
* Android™ and TensorFlow Lite™ are trademarks of Google LLC.
* macOS® is a trademark of Apple Inc.

## License

This project is distributed under the software licenses in [LICENSES](LICENSES) directory.
The licenses of supported models can be seen in [Supported Models section](#supported-models).

