<!--
    SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->


# LLM library

<!-- TOC -->
* [LLM library](#llm-library)
  * [Prerequisites](#prerequisites)
  * [Quick start](#quick-start)
  * [Cross Compilation for Android and Aarch64](#cross-compilation-for-android-and-aarch64)
  * [To build an executable benchmark binary](#to-build-an-executable-benchmark-binary)
  * [Supported Platforms & cmake presets](#supported-platforms--cmake-presets)
  * [Configuration options](#configuration-options)
    * [Conditional options](#conditional-options)
      * [llama cpp options](#llama-cpp-options)
      * [onnxruntime genai options](#onnxruntime-genai-options)
      * [mediapipe options](#mediapipe-options)
      * [mnn options](#mnn-options)
    * [Supported Models](#supported-models)
      * [llama cpp model](#llama-cpp-model)
        * [llama cpp multimodal](#llama-cpp-multimodal)
      * [onnxruntime genai model](#onnxruntime-genai-model)
      * [mediapipe model](#mediapipe-model)
      * [mnn model](#mnn-model)
        * [mnn multimodal](#mnn-multimodal)
      * [Aarch64 target with SME](#aarch64-target-with-sme)
    * [To Build for macOS](#to-build-for-macos)
    * [llama cpp](#llama-cpp)
    * [onnxruntime genai](#onnxruntime-genai)
    * [mnn](#mnn)
    * [arm llm benchmark](#arm-llm-benchmark)
  * [Troubleshooting](#Troubleshooting)
  * [Contributions](#contributions)
  * [Trademarks](#trademarks)
  * [License](#license)
<!-- TOC -->

This repo is designed for building an
[Arm® KleidiAI™](https://www.arm.com/markets/artificial-intelligence/software/kleidi)
enabled LLM library using CMake build system. It intends to provide an abstraction for different Machine Learning
frameworks/backends that Arm® KleidiAI™ kernels have been integrated into.
Currently, it supports [llama.cpp](https://github.com/ggml-org/llama.cpp), [mediapipe](https://github.com/google-ai-edge/mediapipe),
[onnxruntime-genai](https://github.com/microsoft/onnxruntime-genai), and [MNN](https://github.com/alibaba/MNN) backends.
The backend library (selected at CMake configuration stage) is wrapped by this project's thin C++ layer that could be used
directly for testing and evaluations. However, JNI bindings are also provided for developers targeting Android™ based
applications.

## Prerequisites

* A Linux®-based operating system is recommended (this repo is tested on Ubuntu® 22.04.4 LTS)
* An Android™ or Linux® device with an Arm® CPU is recommended as a deployment target, but this
  library can be built for any native machine.
* CMake 3.28 or above installed
* Python 3.9 or above installed, python is used to download test resources and models
* Android™ NDK (if building for Android™). Minimum version: 29.0.14206865 is recommended and can be downloaded
  from [here](https://developer.android.com/ndk/downloads)
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

## Cross Compilation for Android and Aarch64

Cross compilation is also supported allowing the project to build binaries targeted to an OS/CPU architecture different from the host/build machine. For example it is possible to build the project on a Linux x86_64 platform and build binaries for Android™:

```shell
cmake --preset=x-android-aarch64  -B build 
cmake --build ./build
```

However, the binaries would need to be uploaded to an Android™ device to exercise the tests.

To target Linux-aarch64:

```shell
cmake --preset=x-linux-aarch64  -B build -DCPU_ARCH=Armv8.2_3
cmake --build ./build
```
*-DCPU_ARCH* must be specified for all linux-aarch64 targets (including native when run on linux-aarch64).

See the section below for additional cross-compilation options. 

## To build an executable benchmark binary

To build a standalone benchmark binary add the configuration option `-DBUILD_BENCHMARK=ON` to any of the build
commands above. For example:

On Aarch-64
```shell
cmake -B build --preset=native -DCPU_ARCH=Armv8.2_4 -DBUILD_BENCHMARK=ON
cmake --build ./build
```

## Supported Platforms & cmake presets

The supported build platforms and cmake presets matrix is given below. 
The cmake presets (aka build target) are give in the first column and build platform are given in the first row. 
So for example native builds are have been tested on Linux-x86_64, Linux-aarch64 & macOS-aarch64. While x-android-aarch64 (targets Android™ devices running on aarch64) builds are only tested on Linux-x86_64 & macOS-aarch64.

|  cmake-preset / Host Platform  | Linux-x86_64| Linux-aarch64                      | macOS-aarch64 | Android™ |
|--------------------------------------|---------------|------------------------------------|---------------|---------|
| native                               | ✅            | ✅ *                              | ✅            | -      |
| x-android-aarch64                    | ✅            | -                                 | ✅            | -      |
| x-linux-aarch64                      | ✅            | ✅ †                              | -            | -      |


 \* Linux-aarch64 requires the additional CPU_ARCH build flag, see configuration options below    
 † Use 'native' preset

## Configuration options

Configuration options are divided into 2 parts. The first part (what is covered in this section) is the overall project configuration. The second part covers configuration options relating to the specific LLM framework being used, e.g. llama.cpp/ ONNX or MediaPipe, these items are covered in the sections that follow. 

Configuration option can be used with cmake presets.

For example aarch64 CPU hardware acceleration can be disabled by setting USE_KLEIDIAI=OFF, e.g.
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

Details on additional build options is given below: 

Flag name | Default | Values | Description |
|---|---|---|---|
| LLM_FRAMEWORK | llama.cpp | llama.cpp / mediapipe / onnxruntime-genai / mnn | Specifies the backend framework to be used. |
| BUILD_DEBUG | OFF | ON/OFF | If set to ON a debug build is configured. |
| ENABLE_STREAMLINE | OFF | ON/OFF | Enables Arm Streamline timeline annotations for analyzing LLM initialization, encode, decode, and control-path performance. |
| BUILD_LLM_TESTING | ON | ON/OFF | Builds the project's functional tests when ON. |
| BUILD_BENCHMARK | OFF | ON/OFF | Builds the framework's benchmark binaries and arm-llm-bench-cli for the project when ON. |
| BUILD_JNI_LIB| ON | ON/OFF | Builds the JNI bindings for the project. |
| LOG_LEVEL | INFO/DEBUG | DEBUG, INFO, WARN &  ERROR | For BUILD_DEBUG=OFF the default value is INFO. For BUILD_DEBUG=ON, the default value is DEBUG. |
| USE_KLEIDIAI | ON | ON/OFF | Build the project with KLEIDIAI CPU optimizations; if set to OFF, optimizations are turned off. |
| CPU_ARCH | Not defined | Armv8.2_1, Armv8.2_2, Armv8.2_3, Armv8.2_4, Armv8.2_5, Armv8.6_1, Armv8.6_2, Armv9.2_1, Armv9.2_2 | Sets the target ISA architecture (AArch64). Not all targets support this flag. Only supported with LLM_FRAMEWORK=llama.cpp when targeting linux-aarch64 only. |

The table below gives the mapping of CPU_ARCH flags to Arm CPU features

| CPU_ARCH     | C/C++ compiler flags                             |
|--------------|--------------------------------------------------|
| Armv8.2_1    | -march=armv8.2-a+dotprod                       |
| Armv8.2_2    | -march=armv8.2-a+dotprod+fp16                  |
| Armv8.2_3    | -march=armv8.2-a+dotprod+fp16+sve              |
| Armv8.2_4    | -march=armv8.2-a+dotprod+i8mm                  |
| Armv8.2_5    | -march=armv8.2-a+dotprod+i8mm+sve+sme          |
| Armv8.6_1    | -march=armv8.6-a+dotprod+fp16+sve+i8mm         |
| Armv8.6_2    | -march=armv8.6-a+dotprod+fp16+sve+i8mm+sve2    |
| armv9.0_1    | -march=armv9.2-a+dotprod+fp16+nosve+i8mm+sme   |
| armv9.2_1    | -march=armv9.2-a+dotprod+fp16+nosve+i8mm+sme   |
| armv9.2_2    | -march=armv9.2-a+dotprod+fp16+nosve+i8mm+sme   |


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


### Supported Models

| Framework / Backend    | Supported Models                           | Licenses                                                                                                                                                                                                                                       |
|------------------------|--------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **llama.cpp**          | `phi-2`<br/>`qwen-2-VL`<br/>`llama-3.2-1B` | [mit](https://huggingface.co/microsoft/phi-2/blob/main/LICENSE)<br/> [apache-2.0](https://huggingface.co/Qwen/Qwen2-VL-2B-Instruct/blob/main/LICENSE)<br/> [Llama-3.2-1B](https://huggingface.co/meta-llama/Llama-3.2-1B/blob/main/LICENSE.txt) |
| **onnxruntime-genai**  | `phi4-mini-instruct`                       | [mit](https://huggingface.co/microsoft/Phi-4-mini-instruct/blob/main/LICENSE)                                                                                                                                                                  |
| **mediapipe**          | `gemma-2B`                                 | [Gemma](https://www.kaggle.com/models/google/gemma/license/consent)                                                                                                                                                                             |
| **mnn**                | `qwen-2.5-VL`<br/>`llama-3.2-1B`           | [apache-2.0](https://huggingface.co/Qwen/Qwen2.5-VL-3B-Instruct/blob/main/LICENSE)<br/> [Llama-3.2-1B](https://huggingface.co/meta-llama/Llama-3.2-1B/blob/main/LICENSE.txt) |


#### llama cpp model

This project uses the **phi-2 model** as its default network for `llama.cpp` framework.
The model is distributed using the **Q4_0 quantization format**, which is highly recommended as it
delivers effective inference times by striking a balance between computational efficiency and model performance.

- You can access the model from [Hugging Face](https://huggingface.co/ggml-org/models/blob/main/phi-2/ggml-model-q4_0.gguf).
- The default model configuration is declared in the [`requirements.json`](scripts/py/requirements.json) file.

However, any model supported by the backend library could be used.

> **NOTE**: Currently only Q4_0 models are accelerated by Arm® KleidiAI™ kernels in `llama.cpp`.


##### llama cpp multimodal

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

#### mnn model

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

##### mnn multimodal

The `MNN` backend **also supports multimodal (image + text)** inference in this project.

- You can access the vision model from [Hugging Face](https://huggingface.co/taobao-mnn/Qwen2.5-VL-3B-Instruct-MNN)

**What you need**
- `visual.mnn`: Vision model metadata file generated by the MNN conversion process
- `visual.mnn.weight`: Vision model weight file (used when weights are stored separately)

> **NOTE**: The MNN backend determines whether multimodal mode is active from the `is_visual` field inside the model’s `llm_config.json`.

You can find an example multimodal configuration in [mnnVisionConfig-qwen2.5-3B.json](model_configuration_files/mnnVisionConfig-qwen2.5-3B.json)



#### Aarch64 target with SME

To build for aarch64 Linux system with [Scalable Matrix Extensions](https://developer.arm.com/documentation/109246/0100/SME-Overview/SME-and-SME2):

```shell
cmake -B build --preset=native -DCPU_ARCH=Armv8.2_5
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
### arm llm benchmark

The Arm LLM Benchmark tool (arm-llm-bench-cli) is a framework-agnostic, standalone executable designed to measure both prompt-processing and token-generation performance across all supported LLM backends.

**Supported Frameworks**
- `llama.cpp`
- `onnxruntime-genai`
- `MNN`
- `mediapipe`

Instead of writing your own prompts or relying on framework-specific benchmarking tools, `arm-llm-bench-cli` provides a unified benchmarking pipeline. It automatically detects the backend specified in the LLM configuration file and benchmarks it consistently. The tool repeatedly runs the LLM prompt-processing and token-generation  operations and reports timing and throughput metrics in a standardized format.

> **NOTE**: To build `arm-llm-bench-cli`, enable the benchmarking flag in CMake by setting `-DBUILD_BENCHMARK=ON`.

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
    [ --json-output <path>            | -j <path> ] \
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
    -w 1

Terminal Output:

INFO : Running 1 warmup iteration(s) (results ignored)...

=== ARM LLM Benchmark ===

Parameters:
  model_path         : ./resources_downloaded/models/llama.cpp/llama-3.2-1b/Llama-3.2-1B-Instruct-Q4_0.gguf
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

```

## Troubleshooting

For a list of common errors and their fixes, see TROUBLESHOOTING.md.

## Contributions

The LLM-Runner welcomes contributions. For more details on contributing to the repo please see the [contributors guide](./contributing.md#contributions).

## Trademarks

* Arm® and KleidiAI™ are registered trademarks or trademarks of Arm® Limited (or its subsidiaries) in the US and/or
  elsewhere.
* Android™ and TensorFlow Lite™ are trademarks of Google LLC.
* macOS® is a trademark of Apple Inc.

## License

This project is distributed under the software licenses in [LICENSES](LICENSES) directory.
The licenses of supported models can be seen in [Supported Models section](#supported-models).
