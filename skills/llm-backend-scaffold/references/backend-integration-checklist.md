<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Backend integration checklist (this repo)

Use this as a “did I touch everything?” list when adding a backend.

## Mandatory wiring

- New backend directory: `src/cpp/frameworks/<backend>/`
- Routing: `src/cpp/frameworks/CMakeLists.txt` selects `<backend>` when `LLM_FRAMEWORK=<backend>`
- Enumerate `LLM_FRAMEWORK` values: `scripts/cmake/configuration-options.cmake` (`set(CACHE LLM_FRAMEWORK PROPERTY STRINGS ...)`)
- Model config: at least 1 file in `model_configuration_files/`
- Tests: `test/CMakeLists.txt` includes your config filename under the backend branch

## Typical code touchpoints

- Public interface stays stable: `src/cpp/interface/Llm.hpp`
- Backend adapter lives under: `src/cpp/frameworks/<backend>/`
- Factory selects backend: locate/adjust `LlmFactory` and/or backend registry under `src/cpp/frameworks/` (follow existing patterns)
- Common configuration: `src/cpp/config/` parses config fields used by backends

## Downloads & resources (if applicable)

- Add URLs + hashes: `scripts/py/requirements.json`
- Download logic: `scripts/py/download_resources.py` (avoid changing behavior unless necessary)
- Configure-time downloads: `scripts/cmake/download-resources.cmake`
- Gated models: require `HF_TOKEN` (or `~/.netrc` for `huggingface.co`)

## Build presets (optional)

- If a backend needs special toolchains/flags, update `CMakePresets.json` only if it’s truly preset-worthy; prefer normal cache flags first.

## Docs

- `README.md`: supported frameworks + how to select, supported models, any extra prerequisites
- `TROUBLESHOOTING.md`: platform-specific runtime issues (Android packaging, shared libs, OpenMP notes, etc.)

