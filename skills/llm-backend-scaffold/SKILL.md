---
name: llm-backend-scaffold
description: Scaffold and integrate a new LLM backend/framework into this repository (new subdir under src/cpp/frameworks, wire LLM_FRAMEWORK routing, configuration options, downloads/models, tests, and docs). Use when adding a new framework, porting a backend, or refactoring backend glue code.
---

# LLM backend scaffold

This repo selects a backend via `LLM_FRAMEWORK` at CMake configure time and routes to a backend-specific integration under `src/cpp/frameworks/`.

## Workflow

### 1) Choose the integration shape

- **In-tree backend wrapper** (preferred): implement a thin adapter that satisfies the project’s interface and build it from `src/cpp/frameworks/<new>/`.
- **External dependency**: if building from source, decide where it lives (usually as a subdirectory pulled into the build). Keep it deterministic and pinned.

If the backend needs downloads (models / jars / tools), plan to extend `scripts/py/requirements.json` and keep SHA256s accurate.

### 2) Create the backend directory

- Add `src/cpp/frameworks/<new-backend>/` with a `CMakeLists.txt`.
- Implement the backend adapter classes in that directory following the existing backend patterns.

Reference: `skills/llm-backend-scaffold/references/backend-integration-checklist.md`.

### 3) Wire CMake routing + configuration options

- Route the new backend in `src/cpp/frameworks/CMakeLists.txt` based on `LLM_FRAMEWORK`.
- Update `scripts/cmake/configuration-options.cmake` to include the new `LLM_FRAMEWORK` value in the cache strings list.
- If you need new knobs, add them as cache variables/options in a backend-scoped CMake file and document them in `README.md`.

### 4) Add a model configuration file

- Add at least one JSON config under `model_configuration_files/` for the new backend.
- Ensure the config format is supported by the shared config code in `src/cpp/config/`.

### 5) Ensure tests exercise it

- Update `test/CMakeLists.txt` to include at least one config file in the `CONFIG_FILE_NAME` selection for the new framework (mirrors existing `elseif (${LLM_FRAMEWORK} STREQUAL "...")` cases).
- Build and run: `cmake --preset=native -B build -DLLM_FRAMEWORK=<new>` → `cmake --build ./build` → `ctest --test-dir ./build --output-on-failure`

### 6) Update downloads (if needed)

- Add model/tool entries to `scripts/py/requirements.json` under the appropriate section.
- Compute and set `sha256sum` for every new URL.
- Remember: configure runs downloads via `scripts/cmake/download-resources.cmake` → `scripts/py/download_resources.py`; gated models require `HF_TOKEN` (or `~/.netrc`).

### 7) Document the backend

- Update `README.md` (supported frameworks, build flags, supported models, runtime notes).
- Add troubleshooting notes to `TROUBLESHOOTING.md` if the backend has platform-specific gotchas.
