---
name: llm-add-model-support
description: Add support for a new model in one of this repo's supported LLM frameworks (llama.cpp, onnxruntime-genai, mediapipe, mnn), including model configuration JSON, downloads/sha256 entries, backend-specific prompt/template quirks, tests, and README updates. Use when onboarding a new model, extending supported models list, or fixing model-loading/inference failures for a specific backend.
---

# LLM add model support

This repo’s “model support” typically means: a config in `model_configuration_files/`, model assets available under `resources_downloaded/models/<backend>/...`, and CTest coverage wired in `test/CMakeLists.txt`.

## Workflow

### 1) Pick the target backend and model

- Decide `LLM_FRAMEWORK` and a model identifier (folder name under `resources_downloaded/models/<backend>/`).
- Check if the model is gated on Hugging Face; if yes, ensure `HF_TOKEN` or `~/.netrc` is available for downloads.

### 2) Add/download the model assets (data-first)

Prefer extending the downloads manifest:

- Add entries in `scripts/py/requirements.json` under `models` → `<backend>` → `<model-name>`.
- Every entry must include `url`, `destination`, and `sha256sum`.
- Use `bash skills/llm-add-model-support/scripts/sha256_file.sh <file>` to compute hashes for local files.

If downloads are not desirable (very large / licensing constraints), document how to supply the model manually and keep tests/configs from implicitly requiring the download.

### 3) Add a model configuration JSON

- Add `model_configuration_files/<backend><Text|Vision>Config-<model>.json` (follow existing naming patterns).
- Ensure config fields have safe defaults and are parsed by `src/cpp/config/` (`LlmConfig.*`).
- If you introduce new config keys, update `test/cpp/LlmConfigTest.cpp`.

### 4) Handle backend-specific model quirks (code changes)

Use the minimal set of changes needed for the chosen backend:

- **Prompt template / chat formatting**: update the backend’s query builder or chat adapter under `src/cpp/frameworks/`.
- **Tokenizer/model loading**: ensure the backend can load the model file(s) referenced by the config.
- **Modalities**: confirm `SupportedInputModalities()` matches the model (text-only vs vision).

### 5) Wire tests

Ensure CTest actually runs your config:

```sh
bash skills/llm-add-model-support/scripts/assert_config_listed.sh <your-config.json>
```

If it’s a new backend (or new modality), ensure `test/CMakeLists.txt` has the right branch and config list entries.

### 6) Validate outputs (build + run relevant tests)

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=<backend>
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

If JNI is not relevant to model support changes, isolate:

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=<backend> -DBUILD_JNI_LIB=OFF
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

If a new model causes output drift (tests fail but the model is “reasonable”), revise tests deliberately:

- Prefer making prompts more constrained (e.g., “Answer with a single word”) over weakening assertions.
- Prefer stable, high-signal checks (contains a specific entity like “Paris”) and allow a short whitelist of synonyms/casing when necessary.
- Keep regressions meaningful: avoid assertions that only check “non-empty” unless that’s the contract being tested.
- Validate both directions: a good test should fail if you intentionally break the prompt/model path.

See `skills/llm-add-model-support/references/output-validation.md` for concrete patterns used in this repo’s tests.

### 7) Update docs

- Add the model to `README.md` “Supported Models” table (and link its license).
- If there are new prerequisites or known runtime issues, update `TROUBLESHOOTING.md`.

## References

- Checklist: `skills/llm-add-model-support/references/new-model-checklist.md`
- Backend notes: `skills/llm-add-model-support/references/backend-notes.md`
- Output alignment: `skills/llm-add-model-support/references/output-validation.md`
