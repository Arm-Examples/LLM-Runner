---
name: llm-session-start
description: Run fast “session start / doctor” checks for this repository (toolchain + wiring sanity, framework version report, optional upstream update check), optionally generate a debug bundle, and when needed bump pinned backend framework versions with build+ctest verification. Use at session start or when upgrading llama.cpp/onnxruntime-genai/mnn/executorch pins.
---

# Session start checks

This tool does not automatically run commands on repo initialization just because `AGENTS.md`/skills exist. This skill provides a one-command workflow you can invoke at the start of a session.

Windows note: if `python3` isn’t available, use `python` (or `py -3`) for the scripts below.

For generic build/CTest debugging, prefer `skills/llm-build-and-ctest/` or `skills/llm-debug-test-failures/` instead of this skill.

## Workflow

### 1) Run fast environment + wiring checks (recommended)

```sh
python3 skills/llm-session-start/scripts/start.py build
```

This runs:
- `python3 scripts/dev/llm_doctor.py --build-dir <build-dir>`
- `python3 scripts/dev/framework_versions.py --build-dir <build-dir>`

### 1b) Run doctor checks only (when debugging)

From repo root:

```sh
python3 scripts/dev/llm_doctor.py --build-dir build
```

If there is no `build/` yet, you can still validate test wiring for a specific backend:

```sh
python3 scripts/dev/llm_doctor.py --build-dir build --llm-framework llama.cpp
```

### 2) Optionally check upstream for updates (network)

The upstream update check contacts Git remotes and requires network access. If the process runs in a sandboxed environment, enable network access in the sandbox configuration, restart the process, and verify it works (see `skills/README.md`):

```sh
python3 skills/llm-session-start/scripts/start.py build --network
```

Exit code behavior:
- `0`: no updates detected
- `2`: updates available
- `1`: error checking upstream

If updates are available, see the “Bump pinned framework versions” section below.

### 3) Create a debug bundle for bug reports (optional)

```sh
python3 scripts/dev/collect_debug_bundle.py build .
```

This produces `llm-debug-bundle-<timestamp>.tar.gz` with environment info, CMake logs/cache, and (if available) last CTest logs plus git status/diff.

### 4) Bump pinned framework versions (when upgrading a backend)

This repo pins backend dependencies via cache variables in the backend CMake files under `src/cpp/frameworks/`. Updates may require wrapper changes in `src/cpp/frameworks/<backend>/` if upstream APIs changed.

1) Generate a version report (current pins):

```sh
python3 scripts/dev/framework_versions.py --build-dir build
```

2) Decide update scope:
- Prefer updating **one framework at a time** (e.g., llama.cpp only), then build+ctest, then move on.
- For ONNX, consider whether you need to bump `onnxruntime` and `onnxruntime-genai` together; compatibility can be strict.

3) Check upstream tags/commits (requires network):

```sh
git ls-remote --tags https://github.com/ggerganov/llama.cpp.git | tail
git ls-remote --tags https://github.com/microsoft/onnxruntime.git | tail
git ls-remote --tags https://github.com/microsoft/onnxruntime-genai.git | tail
git ls-remote --tags https://github.com/alibaba/MNN.git | tail
git ls-remote --tags https://github.com/pytorch/executorch.git | tail
```

4) Update the pinned default (repo change):
- `src/cpp/frameworks/llama_cpp/CMakeLists.txt`: `LLAMA_GIT_SHA`
- `src/cpp/frameworks/onnxruntime_genai/CMakeLists.txt`: `ONNXRUNTIME_GIT_TAG`, `ONNXRT_GENAI_GIT_TAG`
- `src/cpp/frameworks/mnn/CMakeLists.txt`: `MNN_GIT_TAG`
- `src/cpp/frameworks/executorch/CMakeLists.txt`: `EXECUTORCH_GIT_TAG`

5) Reconfigure + rebuild + run tests:

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=<backend>
cmake --build ./build --parallel
ctest --test-dir ./build --output-on-failure
```

If JNI is unrelated to the bump, isolate failures:

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=<backend> -DBUILD_JNI_LIB=OFF
cmake --build ./build --parallel
ctest --test-dir ./build --output-on-failure
```

6) Triage wrapper/API breakages:
- Most breakages land in `src/cpp/frameworks/<backend>/`.
- Use `skills/llm-session-start/references/upgrade-triage.md` for common patterns and what to capture in bug reports.

## Notes

If you want this to run automatically before launching the tool, create a shell alias/wrapper outside the repo (see `skills/llm-session-start/references/wrapper-alias.md`).

See `skills/llm-session-start/references/what-it-checks.md` for what doctor checks cover.
