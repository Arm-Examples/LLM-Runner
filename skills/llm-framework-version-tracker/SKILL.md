---
name: llm-framework-version-tracker
description: Track and update the pinned framework versions/commits for this repo’s backends (llama.cpp, onnxruntime + onnxruntime-genai, MNN; mediapipe optional), including generating a version report, safely bumping tags/SHAs, and triaging wrapper/API breakages with build+ctest verification. Use when upgrading backend dependencies, responding to upstream releases, or diagnosing breakages after a version bump.
---

# LLM framework version tracker

This repo pins backend dependencies via cache variables in the backend CMake files under `src/cpp/frameworks/`. Updates may require wrapper changes in `src/cpp/frameworks/<backend>/` if upstream APIs changed.

## Workflow

### 1) Generate a version report (current pins)

```sh
python3 scripts/dev/framework_versions.py --build-dir build
```

Or via the skill wrapper:

```sh
bash skills/llm-framework-version-tracker/scripts/framework_versions.sh build
```

This prints:
- the default pinned tags/SHAs in repo files (and where they live)
- any overrides currently set in `build/CMakeCache.txt` (if present)

### 1b) Check for upstream updates (optional automation)

Manual check:

```sh
python3 scripts/dev/check_framework_updates.py --json
```

Exit codes:
- `0`: no updates detected
- `2`: updates available (action needed)
- `1`: error checking upstream

CI automation:
- `.github/workflows/framework-update-check.yml` runs on a schedule and files/updates an issue when updates are detected.

### 2) Decide update scope

- Prefer updating **one framework at a time** (e.g., llama.cpp only), then build+ctest, then move on.
- For ONNX, consider whether you need to bump `onnxruntime` and `onnxruntime-genai` together; upstream compatibility can be strict.

### 3) Check upstream for a newer tag/commit (requires network)

This step needs network access. In restricted environments, request network approval before running:

```sh
git ls-remote --tags https://github.com/ggerganov/llama.cpp.git | tail
git ls-remote --tags https://github.com/microsoft/onnxruntime.git | tail
git ls-remote --tags https://github.com/microsoft/onnxruntime-genai.git | tail
git ls-remote --tags https://github.com/alibaba/MNN.git | tail
```

If you already know the target tag/SHA from release notes, you can skip this step and go directly to the bump.

### 4) Update the pinned default (repo change)

Edit the backend CMake file and bump the default tag/SHA:
- `src/cpp/frameworks/llama_cpp/CMakeLists.txt`: `LLAMA_GIT_SHA`
- `src/cpp/frameworks/onnxruntime_genai/CMakeLists.txt`: `ONNXRUNTIME_GIT_TAG`, `ONNXRT_GENAI_GIT_TAG`
- `src/cpp/frameworks/mnn/CMakeLists.txt`: `MNN_GIT_TAG`

Keep the variables as cache entries so users/CI can override without patching the repo.

### 5) Reconfigure + rebuild + run tests

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=<backend>
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

If JNI is unrelated to the bump, isolate failures:

```sh
cmake --preset=native -B build -DLLM_FRAMEWORK=<backend> -DBUILD_JNI_LIB=OFF
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

### 6) Fix wrapper/API breakages (if any)

Most breakages land in:
- `src/cpp/frameworks/llama_cpp/`
- `src/cpp/frameworks/onnxruntime_genai/`
- `src/cpp/frameworks/mnn/`

Use `skills/llm-framework-version-tracker/references/upgrade-triage.md` for common patterns and what to capture in bug reports.

### 7) Record the upgrade

- Update `README.md` notes if the repo claims tested versions (it currently does for ONNX + MNN).
- Re-run the version report and include it in the PR description.
