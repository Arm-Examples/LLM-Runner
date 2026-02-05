<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Codex in this repo (how to use it well)

This repo includes **project-local Codex skills** under `skills/` plus conventions in `AGENTS.md` and `SKILLS.md`.

The goal is to make common tasks (build/test, add models, bump framework versions, debug failures) repeatable and safe.

## How skills trigger

Skills trigger automatically based on the `description:` line in each `skills/<name>/SKILL.md`.

To force a skill:
- Mention the skill name explicitly (e.g., “use `llm-build-and-ctest` …”).

Codex does **not** automatically run commands “on repo open”; you must prompt it (or use the optional wrapper described in `skills/llm-session-start/references/wrapper-alias.md`).

## Start every session the same way

Run the standard checks once per session:

```sh
bash skills/llm-session-start/scripts/start.sh build
```

Optional upstream update check (requires network):

```sh
bash skills/llm-session-start/scripts/start.sh build --network
```

What you get:
- environment + wiring status (`llm_doctor.py`)
- current pinned framework versions (`framework_versions.py`)
- optional update suggestions (`check_framework_updates.py`)

## Daily workflows (what to ask Codex)

### Build + test

Use when you changed C++/CMake/tests or need to reproduce CI:
- “Run ctest on native and summarize failures.”
- “Re-run only the failing ctest with verbose output.”

Skill: `skills/llm-build-and-ctest/SKILL.md`

### Debug “it doesn’t build / tests fail”

Use when you’re not sure if it’s tooling, wiring, or build state:
- “Run doctor and tell me what’s wrong.”
- “Create a debug bundle for a bug report.”

Skill: `skills/llm-dev-doctor/SKILL.md`
Tools: `scripts/dev/llm_doctor.py`, `scripts/dev/collect_debug_bundle.sh`

### Add a new model (most common)

Use when onboarding a model, adding config JSON, downloading assets, and ensuring tests still validate correct behavior:
- “Add support for model X for `LLM_FRAMEWORK=llama.cpp`, update configs, and fix tests if outputs drift.”
- “Add a new model config and ensure it’s included in ctest.”

Skills:
- `skills/llm-add-model-support/SKILL.md` (end-to-end model onboarding + output-alignment guidance)
- `skills/llm-add-model-config/SKILL.md` (config + CTest wiring only)

Important: tests in `test/cpp/LlmTest.cpp` are output-based (substring anchors). If a new model is correct but phrasing changes, update tests deliberately (constrain prompts first; keep checks high-signal). See `skills/llm-add-model-support/references/output-validation.md`.

### Add a new backend/framework

Use when adding a new `LLM_FRAMEWORK` option and wiring the wrapper layer:
- “Scaffold a new backend called foo and wire it end-to-end.”

Skill: `skills/llm-backend-scaffold/SKILL.md`

### Change API/features safely

Use when changing the public C++ API or behavior:
- “Add feature X to the `LLM` API and update tests (JNI optional).”

Skill: `skills/llm-change-api-safely/SKILL.md`

### Benchmark work

Use when changing/validating `arm-llm-bench-cli` or backend benchmarks:
- “Build benchmarks and run a smoke check.”

Skill: `skills/llm-benchmark-workflow/SKILL.md`

### Track/upgrade framework versions

Use when checking current pins or bumping upstream dependencies (llama.cpp, onnxruntime(+genai), mnn):
- “Show pinned framework versions and check if updates are available.”
- “Bump llama.cpp to <sha> and fix wrapper breakages.”

Skill: `skills/llm-framework-version-tracker/SKILL.md`
Tooling: `scripts/dev/framework_versions.py`, `scripts/dev/check_framework_updates.py`

## Working with restricted network / approvals

Some operations (checking upstream tags, downloading models) require network access. In interactive environments, Codex will request approval when needed.

Best practice:
- Keep “offline” checks fast (`llm_doctor.py`, `framework_versions.py`).
- Only opt into network checks when you explicitly need them (e.g., `--network`, update checks).

## Where to look (repo map)

- Core C++ API: `src/cpp/interface/Llm.hpp`
- Backend glue: `src/cpp/frameworks/`
- Model configs: `model_configuration_files/`
- Downloads manifest: `scripts/py/requirements.json`
- Tests: `test/` (especially `test/cpp/LlmTest.cpp`, `test/CMakeLists.txt`)
- Build presets: `CMakePresets.json`

## Worked example (real workflow)

### Use case: onboard a new text model for `llama.cpp` and keep tests reliable

Goal: add a new model to the repo’s supported list for `LLM_FRAMEWORK=llama.cpp`, wire it into CTest, and adjust tests only if the new model’s phrasing differs but is still correct.

1) Start the session with fast checks (no network):

```sh
bash skills/llm-session-start/scripts/start.sh build
```

2) Ask Codex to onboard the model end-to-end (this should trigger `llm-add-model-support`):

Suggested prompt:
- “Use `llm-add-model-support` to add llama.cpp support for model `my-new-model` (text-only). Add downloads + sha256, add a config in `model_configuration_files/`, ensure `test/CMakeLists.txt` includes it, and run `ctest` on native.”

What happens / what Codex will touch:
- `scripts/py/requirements.json`: add the new model file(s) URL + `sha256sum`
- `model_configuration_files/`: add `llamaTextConfig-<model>.json`
- `test/CMakeLists.txt`: add the config filename under the `llama.cpp` branch so CTest executes it
- Optionally `src/cpp/config/` if new config keys are introduced

3) If the new model fails output assertions but is still correct (output drift), ask Codex to align tests (still part of `llm-add-model-support`):

Suggested prompt:
- “The new model is correct but `test/cpp/LlmTest.cpp` is failing due to phrasing differences. Constrain the prompts first, then adjust expected anchors/whitelists. Keep the checks high-signal; don’t weaken to ‘non-empty’.”

Guidelines Codex should follow (this repo’s pattern):
- tighten prompts (“Answer with a single word.”) before broadening assertions
- anchor on stable entities (e.g., “Paris”) and allow small synonym lists when justified
- re-run only the relevant test(s) with `ctest -R ... -V` while iterating

4) Prove everything still passes:

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=llama.cpp
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

5) If downloads require network or gated models:
- Provide `HF_TOKEN` (or set up `~/.netrc`) and rerun configure.
- In interactive environments, Codex may ask for network approval when it needs to check upstream or download.
