<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Codex skills (project)

This file is a lightweight index of **project-specific Codex skills** we may add over time to make common tasks repeatable and safe.

See `CODEX.md` for contributor guidance and recommended workflows.

## Implemented skills

- `skills/llm-build-and-ctest/SKILL.md`: Build + run `ctest` via CMake presets; use for “run tests”, “ctest”, “reproduce CI”, “CMake configure/build failed”, “JNI build failing”.
- `skills/llm-backend-scaffold/SKILL.md`: Add a new backend under `src/cpp/frameworks/` and wire `LLM_FRAMEWORK` routing + tests + docs.
- `skills/llm-benchmark-workflow/SKILL.md`: Build/run benchmarking tools (`-DBUILD_BENCHMARK=ON`, `arm-llm-bench-cli`) and diagnose benchmark issues.
- `skills/llm-add-model-config/SKILL.md`: Add/update a config in `model_configuration_files/` and ensure CTest includes it for the right backend.
- `skills/llm-add-model-support/SKILL.md`: Onboard a new model for a backend (downloads/hashes, config JSON, backend quirks, tests, and docs).
- `skills/llm-change-api-safely/SKILL.md`: Change the public C++ API and implementations safely, including optional JNI impact and test updates.
- `skills/llm-dev-doctor/SKILL.md`: Run fast environment/wiring checks and collect a debug bundle for bug reports.
- `skills/llm-framework-version-tracker/SKILL.md`: Track and bump pinned backend framework versions/commits, with build+ctest verification and upgrade triage notes.
- `skills/llm-session-start/SKILL.md`: One-command start-of-session checks (doctor + pinned versions + optional upstream update check).

## Conventions (recommended)

- Store repo-local skills under `skills/<skill-name>/SKILL.md`.
- Keep skills offline-friendly and deterministic by default (avoid downloads unless explicitly requested).
- Prefer “one workflow per skill” (build/test, add backend, update model config, triage failures).
- When a skill needs external access (network/device), document the exact commands and what state they mutate.

## Suggested starter skills (not yet implemented)

- `llm-update-downloads`: Update `scripts/py/requirements.json` entries (URL + `sha256sum`) and validate download logic changes.
- `llm-android-cross-build`: Build with `--preset=x-android-aarch64`, document adb push steps, and common troubleshooting.

## How to add new skills

- Use your Codex “skill creator” workflow to produce `skills/<name>/SKILL.md`.
- Keep the skill short: prerequisites, exact commands, expected outputs, and rollback/cleanup steps.
- Update this `SKILLS.md` with a one-line description and the primary trigger phrases you expect to use.
