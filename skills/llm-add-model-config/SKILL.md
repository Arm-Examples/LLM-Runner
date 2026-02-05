---
name: llm-add-model-config
description: Add or revise a model configuration JSON under model_configuration_files/ and ensure it is exercised by CTest (test/CMakeLists.txt) for the relevant backend. Use when onboarding a new model, changing config schema, updating supported models, or fixing model-related test failures.
---

# LLM add model config

Model configuration files live in `model_configuration_files/` and are referenced by CTest wiring in `test/CMakeLists.txt`.

## Workflow

### 1) Decide which backend owns the config

- `LLM_FRAMEWORK=llama.cpp` configs typically start with `llama...Config-...json`
- `LLM_FRAMEWORK=onnxruntime-genai` configs typically start with `onnxrt...Config-...json`
- `LLM_FRAMEWORK=mediapipe` configs typically start with `mediapipe...Config-...json`
- `LLM_FRAMEWORK=mnn` configs typically start with `mnn...Config-...json`

### 2) Add/update the JSON file

- Add file under `model_configuration_files/`.
- Keep naming consistent with existing files for the backend.
- If you change the schema, update `src/cpp/config/` parsing and tests (`test/cpp/LlmConfigTest.cpp`).

### 3) Ensure CTest includes it

Add the filename to the backend’s `CONFIG_FILE_NAME` list in `test/CMakeLists.txt`.

Optional helper:

```sh
bash skills/llm-add-model-config/scripts/assert_config_listed.sh <your-config.json>
```

### 4) Ensure downloads cover model assets (if applicable)

If the config points at model files that are expected to be available via this repo’s download system:
- Add/update entries in `scripts/py/requirements.json` (URLs + `sha256sum`).
- Remember gated models require `HF_TOKEN` or `~/.netrc`.

### 5) Prove it works

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=<backend>
cmake --build ./build
ctest --test-dir ./build -R llm-cpp-ctest --output-on-failure
```

If the test is slow or model-gated, at least validate:
- The config file exists (CTest will `FATAL_ERROR` if not).
- The relevant model path exists under `resources_downloaded/models/<backend>/...` for your config.
