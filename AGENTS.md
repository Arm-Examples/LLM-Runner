# Agent guide

This repository builds an Arm KleidiAI-enabled LLM wrapper library with a thin, backend-agnostic C++ API and optional JNI bindings. Supported backends are selected at CMake configure time: `llama.cpp`, `onnxruntime-genai`, `mediapipe`, and `mnn`.

## High-signal paths

- `README.md`: build options, supported frameworks/models, basic usage.
- `skills/`: project-local skills for repeatable workflows.
- `CMakePresets.json`: supported build presets.
- `scripts/cmake/`: CMake modules, toolchains, downloads, framework glue.
- `scripts/dev/`: doctor checks, version reports, debug bundle helpers.
- `scripts/py/download_resources.py` and `scripts/py/requirements.json`: deterministic download definitions.
- `model_configuration_files/`: model config JSONs consumed by tests and examples.
- `src/cpp/Llm.cpp` and `src/cpp/LlmJni.cpp`: top-level native and JNI entry points; common init, teardown, and benchmark integration live here.
- `src/cpp/interface/`: public C++ API.
- `src/cpp/config/`: config parsing and schema handling.
- `src/cpp/log/`: shared logging macros, formatting helpers, and generated build metadata reporting.
- `src/cpp/chat/`: prompt templating, query building, and conversation formatting helpers.
- `src/cpp/frameworks/`: backend integrations.
- `src/cpp/benchmark/`: benchmarking pipeline; `LlmBench` adapts the LLM API and `BenchRunner` owns iteration/report formatting.
- `src/cpp/profiling/`: optional Arm Streamline integration and timeline annotations.
- `src/java/`: Java/JNI surface.
- `test/cpp/`: Catch2 coverage for config, logging, benchmark runner, and core wrapper behavior.
- `test/java/`: JNI-facing tests for the Java surface.
- `TROUBLESHOOTING.md`: platform-specific issues and limitations.

## Skills

Project-specific skills live under `skills/`. Use them when the task clearly matches a documented workflow. Keep `AGENTS.md` for durable repo rules; keep detailed procedures in the skills themselves.

## Validation

When a change affects build, test, runtime behavior, CMake, scripts, or public API/configuration, run a local build and CTest before considering the change done:

```sh
cmake --preset=native -B build
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

If JNI is not relevant, it is reasonable to iterate with `-DBUILD_JNI_LIB=OFF`.

## Docs

Update `README.md` when a change affects something users or reviewers need to know how to build, configure, run, or use. Update `TROUBLESHOOTING.md` for platform-specific issues or new limitations.

## Logging

- Use the shared logging macros in `src/cpp/log/Logger.hpp`: `LOG_ERROR`, `LOG_WARN`, `LOG_INF`, `LOG_DEBUG`, and `LOG_BUILD_INFO`.
- Prefer `THROW_ERROR` and `THROW_INVALID_ARGUMENT` for error paths that should log and throw together.
- Do not add new raw `printf`, `fprintf`, `std::cout`, or `std::cerr` calls in native code unless there is a clear file-local reason.
- Keep log messages concise and structured. Include framework/backend names and relevant paths or config values when diagnosing initialization or runtime failures.
- Route new native/JNI diagnostics through `Logger.hpp` so they appear consistently in CLI output and Android logcat.

## Benchmarking

- Put benchmark report and output formatting changes in `src/cpp/benchmark/BenchRunner.*`.
- Put benchmark adapter/runtime interaction changes in `src/cpp/benchmark/LlmBench.*`.

## JNI

- Keep JNI-visible behavior in `src/cpp/LlmJni.cpp` aligned with the Java surface in `src/java/com/arm/Llm.java`.

## Build metadata

- Route build/version metadata changes through `src/cpp/log/BuildInfo.*` and `src/cpp/log/LlmBuildInfoConfig.hpp.in` instead of hardcoding values in multiple places.

## Downloads

Configure may trigger resource downloads through `scripts/cmake/download-resources.cmake` and `scripts/py/download_resources.py`. Some models are gated on Hugging Face.

Preferred token sources:
- `HF_TOKEN`
- `~/.netrc` entry for `huggingface.co`

Do not commit anything from `resources_downloaded/`. Avoid introducing changes that force large re-downloads unless explicitly needed.

## Patch hygiene

- Do not commit `build*/`, `resources_downloaded/`, or `download.log`.
- Avoid checking in large binaries or model artifacts; prefer stable URLs and `sha256sum` updates in `scripts/py/requirements.json`.
- Treat SPDX maintenance as part of the same change, not follow-up work.
- Add the standard repo SPDX header to new source/doc/script files that support comments.
- When editing an existing source/doc/script file that supports comments, ensure the SPDX header is present and the year/range is current. Edit the year only; do not change holder or license text.
- Do not inject SPDX comments into formats that would break consumers, such as JSON. Use a repo-approved sidecar or documentation approach instead.

## Notes

- If `python3` is unavailable, use `python` or `py -3`.
- Useful debug helpers:
  - `python3 scripts/dev/llm_doctor.py --build-dir build`
  - `python3 scripts/dev/collect_debug_bundle.py build .`
