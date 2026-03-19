<!--
SPDX-FileCopyrightText: 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
SPDX-License-Identifier: Apache-2.0
-->

# Architecture Overview

<!-- TOC -->
* [Architecture Overview](#architecture-overview)
* [Inputs and Outputs](#inputs-and-outputs)
* [Repository Layout](#repository-layout)
<!-- /TOC -->

This lightweight library provides a single C++ & Java API to various LLM frameworks.
The project uses **CMake presets** to support native x86, macOS or aarch64 builds and cross-compilation for Android or linux-aarch64.

```mermaid
graph TD
    Prompt[Input Prompt<br/>(text or text+image)]
    --> App[Application<br/>(C++ API or JNI)]

    App --> LLMRunner[LLM-Runner API]

    LLMRunner --> Backend[Selected Backend<br/>llama.cpp | onnxruntime-genai | Mediapipe | MNN]

    Backend --> Inference[Inference with model + config]

    Inference --> KleidiAI[Arm KleidiAI kernels (if enabled)]

    KleidiAI --> Output[Generated tokens / text]
```
---
Typical Flow:
1. A prompt (text, or text+image for multimodal backends) is provided to the LLM-Runner library.
2. The wrapper selects the configured backend.
3. The backend performs inference using the loaded model and configuration.
4. If enabled, **Arm® KleidiAI™ kernels accelerate key operations** on supported Arm CPUs.
5. The backend returns generated tokens / text.
6. Applications receive the result either through:
    - the **C++ API**, or
    - the **JNI interface** on Android™.

---

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
