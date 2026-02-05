<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Codex agent guide

This repository builds an Arm KleidiAI-enabled LLM wrapper library with a thin, backend-agnostic C++ API (and optional JNI bindings). It supports multiple backends selected at CMake configure time: `llama.cpp`, `onnxruntime-genai`, `mediapipe`, and `mnn`.

## Repo map (high-signal)

- `CMakePresets.json`: Supported build presets (`native`, `x-android-aarch64`, `x-linux-aarch64`).
- `scripts/cmake/`: CMake modules (configuration options, toolchains, downloads, framework glue).
- `scripts/py/download_resources.py` + `scripts/py/requirements.json`: Resource/model download definition + logic.
- `resources_downloaded/`: Download destination for models and test utilities (gitignored; created/updated by configure).
- `model_configuration_files/`: Model config JSONs consumed by tests and examples.
- `src/cpp/interface/`: Public C++ surface (`LLM` in `src/cpp/interface/Llm.hpp`).
- `src/cpp/frameworks/`: Backend selection; subdirs contain backend integrations.
- `test/`: Catch2-based C++ tests plus optional JNI tests (`test/CMakeLists.txt` wires both).
- `TROUBLESHOOTING.md`: Known platform issues (Android/macOS).

## Build & test (native)

Typical local flow:

```sh
cmake --preset=native -B build
cmake --build ./build
ctest --test-dir ./build
```

Notes:
- CMake configure triggers resource downloads (models, jars, etc.) unless already present in `resources_downloaded/`.
- JNI is enabled by default (`BUILD_JNI_LIB=ON` in the `base` preset).

## Start-of-session checks (recommended)

Codex does not automatically run commands on repo open; run this once at the start of a session:

```sh
bash skills/llm-session-start/scripts/start.sh build
```

To also check upstream for pinned framework updates (requires network):

```sh
bash skills/llm-session-start/scripts/start.sh build --network
```

## Downloads and gated models (important for automation)

During configure, `scripts/cmake/download-resources.cmake` runs `scripts/py/download_resources.py`. Some models may be gated on Hugging Face.

Preferred token sources:
- `HF_TOKEN` environment variable (preferred for CI/automation)
- `~/.netrc` entry for `huggingface.co`

When working with a restricted-network environment, avoid introducing changes that require re-downloading large artifacts unless explicitly needed, and keep anything under `resources_downloaded/` out of patches.

## Common change points

- Add/update a model config:
  - Edit/add JSON in `model_configuration_files/`.
  - Ensure tests reference it (see `test/CMakeLists.txt`, `CONFIG_FILE_NAME` list per backend).
- Add a new backend integration:
  - Add a new subdirectory under `src/cpp/frameworks/`.
  - Update `src/cpp/frameworks/CMakeLists.txt` to route `LLM_FRAMEWORK` to the new backend.
  - Update `README.md` to document build options, supported models, and usage.
- Change download requirements:
  - Update `scripts/py/requirements.json` (URLs + `sha256sum`) and keep `download_resources.py` behavior deterministic.

## Patch hygiene (what not to touch)

- Do not commit anything from `build*/`, `resources_downloaded/`, or `download.log` (all are gitignored).
- Avoid checking in large binaries/models; prefer updating `scripts/py/requirements.json` with stable URLs and hashes.

## Debug helpers

- Fast environment/wiring checks: `python3 scripts/dev/llm_doctor.py --build-dir build`
- Bug-report bundle: `bash scripts/dev/collect_debug_bundle.sh build .`
