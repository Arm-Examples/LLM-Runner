<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# New model checklist (this repo)

## Minimum definition of “supported”

- Model assets available (downloaded or manually placed) under `resources_downloaded/models/<backend>/...`
- A config JSON exists under `model_configuration_files/` and points at the model assets correctly
- `ctest` has a path that exercises that config (via `test/CMakeLists.txt`)
- `README.md` documents the model + license and any backend-specific flags

## Files commonly touched

- Config JSON: `model_configuration_files/*.json`
- Downloads manifest: `scripts/py/requirements.json` (+ ensure `sha256sum` is correct)
- Download runner: `scripts/py/download_resources.py` (avoid changing unless necessary)
- Tests: `test/CMakeLists.txt`, `test/cpp/LlmTest.cpp`, `test/cpp/LlmConfigTest.cpp`
- Backend adapters: `src/cpp/frameworks/`
- Docs: `README.md`, `TROUBLESHOOTING.md`

## Gotchas

- Gated models: require `HF_TOKEN` or `~/.netrc` for `huggingface.co`
- Don’t commit binaries/models; keep artifacts under `resources_downloaded/` (gitignored)
- If the model is huge, consider not enabling it by default in tests; document the opt-in path

