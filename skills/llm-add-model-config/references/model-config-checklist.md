<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Model config checklist (this repo)

## Places that usually change together

- Config JSON: `model_configuration_files/<name>.json`
- Config parsing/schema: `src/cpp/config/` (`LlmConfig.*`)
- Tests:
  - `test/CMakeLists.txt` (`CONFIG_FILE_NAME` selection per backend)
  - `test/cpp/LlmConfigTest.cpp` for schema/validation coverage

## Downloads (if the model is expected to be fetched automatically)

- `scripts/py/requirements.json`:
  - Add the new model under `models` → `<backend>` → `<model-name>`
  - Include stable URL + `sha256sum`
- `scripts/py/download_resources.py`:
  - Prefer not to change logic; extend data first

## Test expectations

- CTest enumerates config filenames and passes:
  - `--config <path/to/model_configuration_files/...>`
  - `--model-root <resources_downloaded/models>`
- If you add a config but do not want it in CI by default, document why and how to run it (avoid “surprise downloads”).

