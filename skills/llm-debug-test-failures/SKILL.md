---
name: llm-debug-test-failures
description: Debug failing LLM integration tests caused by model output drift, incorrect context/runtime parameters (contextSize, batchSize, threads), prompt/template mismatches, or backend/framework regressions. Use when tests fail and you need to see the model response, reproduce a single failing CTest, or trace issues into src/cpp/frameworks (llama.cpp, onnxruntime-genai, mnn, executorch).
---

# Debug failing model/tests

Use this when `llm-cpp-ctest-*` fails due to:
- model output drift (expected anchors not found)
- context/runtime parameters (context full, truncation, batch sizing)
- prompt/template issues (chat formatting differences)
- backend/framework regressions

Windows note: if `python3` isn’t available, use `python` (or `py -3`) for the scripts below.

## Workflow

### 1) Re-run the failing test with maximum signal

From the build dir you used:

```sh
ctest --test-dir ./build --output-on-failure -V
```

To run just one failing test, use the test name shown by CTest:

```sh
ctest --test-dir ./build -R llm-cpp-ctest-<config> -V --output-on-failure
```

Tip: `ctest --test-dir ./build -N` lists tests without running them.

Optional helper (run once, then rerun only failing tests verbosely):

```sh
python3 skills/llm-debug-test-failures/scripts/rerun_failing_ctest.py build --cpp-transcript ./llm-test-transcript.txt
```

### 2) Inspect the model response and config/runtime values

The C++ and JNI tests print the model response and key context when an assertion fails.

If you want the C++ tests to print prompts/responses even when assertions pass, set:
- `LLM_TEST_DEBUG_RESPONSES=1` (environment variable), or
- add `--debug-responses` when running the `llm-cpp-tests` binary directly.

If you want a file you can attach to bugs/PRs (works even when CTest output is hard to read), set:
- C++: `LLM_TEST_TRANSCRIPT_PATH=./llm-test-transcript.txt` (or pass `--transcript <path>`)
- JNI: add `-Dllm.tests.transcript=./llm-jni-transcript.txt` to the Java command

For JNI tests, you can also force printing prompts/responses even when tests pass by adding `-Dllm.tests.debug=true` to the Java command (copy it from `ctest -V` output and add the flag).
If the failure is “output drift” but the answer is still correct:
- constrain the prompt first (“Answer with a single word.”)
- then update the expected anchors (keep them high-signal)

Reference: `skills/llm-add-model-support/references/output-validation.md`.

### 3) Validate the config file and model paths

- Confirm the config JSON exists under `model_configuration_files/` and is referenced by `test/CMakeLists.txt` (`CONFIG_FILE_NAME` list).
- Confirm `--model-root` points at `resources_downloaded/models` (CTest passes this automatically).
- If the error is “context is full”, inspect `contextSize` + `batchSize` in the config JSON and any test overrides.

### 4) Trace into the backend integration (if needed)

Most backend-specific issues live under:
- `src/cpp/frameworks/llama_cpp/`
- `src/cpp/frameworks/onnxruntime_genai/`
- `src/cpp/frameworks/mnn/`
- `src/cpp/frameworks/executorch/`

Look for:
- prompt/template construction differences
- model loading paths derived from config fields
- modality handling (text vs vision) and batch/context sizing

### 5) Trace into the upstream framework source (when it looks like a framework bug)

If the wrapper code looks correct but behavior/crashes originate in the underlying framework, use the build tree’s fetched sources.

1) Identify the backend and pinned revision:
   - From the failing test output: use the printed framework/config summary.
   - Or run: `python3 scripts/dev/framework_versions.py --build-dir build`
   - The pin and local source path are also defined in the backend’s `CMakeLists.txt`:
     - llama.cpp: `src/cpp/frameworks/llama_cpp/CMakeLists.txt` (`LLAMA_GIT_SHA`, `LLAMA_SRC_DIR`)
     - MNN: `src/cpp/frameworks/mnn/CMakeLists.txt` (`MNN_GIT_TAG`, `MNN_SRC_DIR`)
     - ONNX: `src/cpp/frameworks/onnxruntime_genai/CMakeLists.txt` (`ONNXRUNTIME_GIT_TAG`, `ONNXRT_GENAI_GIT_TAG`, `*_SRC_DIR`)
     - ExecuTorch: `src/cpp/frameworks/executorch/CMakeLists.txt` (`EXECUTORCH_GIT_TAG`, `EXECUTORCH_SRC_DIR`)

2) Open the fetched upstream code under your build directory (default locations):
   - llama.cpp: `build/llama.cpp/`
   - MNN: `build/mnn/`
   - onnxruntime: `build/onnxruntime/`
   - onnxruntime-genai: `build/onnxruntime-genai/`
   - executorch: `build/_deps/executorch/`

3) Connect the dots from wrapper → upstream:
   - Start at the wrapper implementation (e.g. `src/cpp/frameworks/mnn/MnnImpl.cpp`) and follow the calls into upstream headers/sources (includes typically reference the fetched `*_SRC_DIR`).
   - Use a local search tool to find the symbol or error string in upstream sources (e.g. `grep -RIn -- "<symbol-or-error>" build/mnn`).

4) Decide “wrapper bug vs framework bug”:
   - Wrapper bug signals: incorrect mapping of `contextSize`/`batchSize`, wrong model path, wrong prompt template, modality mismatch, or misuse of the framework API.
   - Framework bug signals: crash/assert inside upstream code with correct inputs, regression tied to a version bump, or behavior that contradicts framework docs for the pinned revision.

Practical heuristics (fast triage):

- If **the model won’t load**:
  - Wrapper-side first:
    - Does `LlmConfig` expand `llmModelName` relative to `--model-root` the way the backend expects?
    - Is the config pointing at a file vs a directory (some frameworks expect a folder with multiple artifacts)?
    - Are optional artifacts (e.g. projection model for vision) set/expanded correctly?
  - Framework-side likely when:
    - all paths exist and are readable, but load fails with a framework internal error, assert, or crash.

- If you hit **“context is full” / truncation**:
  - Wrapper-side first:
    - Verify the test/config values printed in the summary: `contextSize`, `batchSize`, and any test overrides.
    - Check whether the wrapper counts tokens/bytes differently than the framework (off-by-one style issues), or whether it re-encodes prior chat turns unexpectedly.
  - Framework-side likely when:
    - the same prompt/config works on a prior pinned revision but fails on the new one (regression after bump).

- If the failure is **output drift (anchors not found)**:
  - Wrapper-side first:
    - Prompt/template formatting differences (chat template application, missing system prompt, stopwords).
    - Wrong backend selected (confirm `framework=` in the config summary).
    - Model mismatch (a different model artifact is being loaded than expected).
  - Framework-side likely when:
    - identical prompt/config/model on the same revision produces unstable output across runs (nondeterminism), or a known decoding change landed upstream.

- If the failure is **crash / SIGSEGV / abort**:
  - Wrapper-side first:
    - invalid inputs (null/empty strings, empty model path, missing image path, bad tensor sizes)
    - lifetime/order issues (free/reset during active decode; encode called with unexpected `isFirstMessage` sequencing)
  - Framework-side likely when:
    - crash happens inside fetched upstream sources with valid inputs, especially reproducible with a minimal prompt.

Backend “where to look first”:
- `llama.cpp`: wrapper chat template/tokenization glue in `src/cpp/frameworks/llama_cpp/`, fetched sources in `build/llama.cpp/`
- `mnn`: model artifact layout + file IO in `src/cpp/frameworks/mnn/`, fetched sources in `build/mnn/`
- `onnxruntime-genai`: session/init + provider config in `src/cpp/frameworks/onnxruntime_genai/`, fetched sources in `build/onnxruntime*/`
- `executorch`: runtime/session integration in `src/cpp/frameworks/executorch/`, fetched sources in `build/_deps/executorch/`
