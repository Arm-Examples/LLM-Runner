<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
SPDX-License-Identifier: Apache-2.0
-->

# Integration Guide

This guide explains how to integrate the **LLM-Runner library** into your own applications or products.

The LLM-Runner library provides a lightweight C++ abstraction layer over the selected backend (`llama.cpp`, `onnxruntime-genai`, `mediapipe`, or `MNN`) and optionally enables Arm® KleidiAI™ acceleration. The library can be embedded into native applications or accessed from Android via JNI.

---

## 1) Choose API: C++ or Java/Kotlin

Select the API that best fits your application.


| Mode        | When to use it | Output                                                         |
|-------------| --- |----------------------------------------------------------------|
| C++ API     | Native applications and services | Static or shared library                                       |
| JNI API     | Android applications | Java / Kotlin API                                              |
| CLI samples | Quick evaluation and benchmarking | `arm-llm-bench-cli`,`llama-cli`, `model_benchmark`, `llm_bench` |

---

## 2) Build the library

Start with a CMake preset and configure the outputs required by your application.

### Native build

```shell
cmake -B build --preset=native
cmake --build ./build
```

### Android build

```shell
cmake -B build --preset=x-android-aarch64
cmake --build ./build
```

When targeting the `llama.cpp` LLM backend and Android (`--preset=x-android-aarch64`), `BUILD_SHARED_LIBS=ON` is automatically configured to ensure the build generates shared libraries for runtime loading.

## 2) Package model assets

The runtime requires backend-compatible model assets.

The default models can be downloaded using:

```shell
python scripts/py/download_resources.py
```

Default model configuration files are stored in `model_configuration_files/`, and the download list is defined in `scripts/py/requirements.json`.

---

## 3) Embed in your application

The public C++ API lives under `src/cpp/interface/`. The JNI surface is under `src/java/`. Use the configuration JSONs under `model_configuration_files/` to select the model and runtime settings appropriate for your integration.

Minimal integration checklist:

- Link the library into your build system (e.g., `target_link_libraries` in CMake).
- Ensure the model file is accessible at runtime.
- Initialize the backend before submitting prompt.
- Log and handle errors during backend initialization and inference.

Example uses default llama.cpp backend but can be adapted to other frameworks. 
See: [`src/cpp/benchmark/main.cpp`](../src/cpp/benchmark/main.cpp)

Example embedding (CMake + C++):

```cmake
# CMakeLists.txt
add_executable(my_app main.cpp)
set(BUILD_LLM_TESTING OFF)
add_subdirectory(/path/to/LLM-Runner llm_runner)
target_link_libraries(my_app PRIVATE arm-llm-cpp)
```

## Standalone App Build

This section shows a minimal application that embeds the LLM-Runner library and runs a single prompt.
Note: Paths default to expected locations of llama.cpp model and config file but positional arguments can be used to override. 

### 1) Create `main.cpp`

Create `main.cpp` at the repo root with the following contents:

```cpp
//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "Llm.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
    const std::string defaultPrompt = "What is the capital of France?";
    const std::string defaultModelPath =
        "/absolute/path/to/Llama-3.2-1B-Instruct-Q4_0.gguf";
    const std::string defaultConfigPath =
        "/absolute/path/to/llamaTextConfig-llama-3.2-1B.json";

    const std::string prompt = (argc > 1) ? argv[1] : defaultPrompt;
    const std::string modelPath = (argc > 2) ? argv[2] : defaultModelPath;
    const std::string configPath = (argc > 3) ? argv[3] : defaultConfigPath;

    try {
        std::ifstream configFile(configPath);
        if (!configFile) {
            std::cerr << "Failed to open config file: " << configPath << std::endl;
            return 1;
        }

        std::stringstream buffer;
        buffer << configFile.rdbuf();
        LlmConfig config(buffer.str());
        config.SetConfigString(LlmConfig::ConfigParam::LlmModelName, modelPath);

        LLM llm{};
        llm.LlmInit(config);

        
        LlmChat::Payload payload{prompt, "", true};
        llm.Encode(payload);

        while (llm.GetChatProgress() < 100) {
            std::string tok = llm.NextToken();
            if (tok == LLM::endToken) {
                break;
            }
            std::cout << tok;
        }
        std::cout << std::endl;

        llm.FreeLlm();
    } catch (const std::exception& e) {
        std::cerr << "LLM run failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### 2) Create `my_app/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.27)
project(my_app LANGUAGES CXX)

add_executable(my_app main.cpp)
add_subdirectory(/path/to/LLM-Runner llm_runner)
target_link_libraries(my_app PRIVATE arm-llm-cpp)
target_compile_features(my_app PRIVATE cxx_std_17)
```

### 3) Configure + Build

Note: Framework selection is compile-time and you will need to rebuild for other supported frameworks and adjust models and config files accordingly.

```bash
cmake -S my_app -B my_app/build \
  -DLLM_FRAMEWORK=llama.cpp \
  -DBUILD_LLM_TESTING=OFF \
  -DBUILD_BENCHMARK=OFF \
  -DBUILD_JNI_LIB=OFF
cmake --build my_app/build
```

### 4) Run

```bash
./my_app/build/my_app \
  --prompt "What is the capital of France?" \
  --model resources_downloaded/models/llama.cpp/llama32_1B_Q4_KM_model.gguf \
  --config model_configuration_files/llamaTextConfig-llama-3.2-1B.json
```

Expected output:

```text
large-language-models$ ./my_app/build/my_app \
>   --prompt "What is the capital of France?" \
>   --model resources_downloaded/models/llama.cpp/llama32_1B_Q4_KM_model.gguf \
>   --config model_configuration_files/llamaTextConfig-llama-3.2-1B.json
INFO : my_app version=0.0.1 git_sha=e5704f9eb88d build_timestamp_utc=2026-03-26T14:17:45Z framework=llama.cpp framework_revisions=[llama.cpp=b7870]
INFO : Initializing LLM with framework='llama.cpp'
WARN : llama_context: n_ctx_seq (2048) < n_ctx_train (131072) -- the full capacity of the model will not be utilized

INFO : LLM initialization complete using framework='llama.cpp'
The capital of France is Paris.
```
