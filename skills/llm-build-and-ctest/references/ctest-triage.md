<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# CTest triage notes (this repo)

## First-pass checklist

- Confirm preset and cache:
  - Read `build/CMakeCache.txt` for `LLM_FRAMEWORK`, `BUILD_JNI_LIB`, `USE_KLEIDIAI`, `CPU_ARCH`.
- Confirm tool versions:
  - `cmake --version`
  - `python3 --version`
  - If JNI enabled: `java -version` and `echo "$JAVA_HOME"`

## Run only what is needed

- List tests:
  - `ctest --test-dir ./build -N`
- Run only the failing test(s):
  - `ctest --test-dir ./build -R <regex> -V --output-on-failure`
- Re-run only failures:
  - `ctest --test-dir ./build --rerun-failed --output-on-failure`

## Common failure sources

- Downloads / gated models:
  - Configure runs a downloads step; ensure `HF_TOKEN` (or `~/.netrc` for `huggingface.co`) is present if the chosen model is gated.
  - Inspect `download.log` if present (it is gitignored).
- JNI toolchain:
  - Retry with `-DBUILD_JNI_LIB=OFF` to isolate issues.
  - If `jni.h` is missing, set `JAVA_HOME` to a compatible JDK and re-configure.
- Shared library discovery (Android / on-device runs):
  - Ensure backend shared libs and executables are in the same directory; use `LD_LIBRARY_PATH` where appropriate (see `README.md` and `TROUBLESHOOTING.md`).

## Build-system logs worth capturing in bug reports

- `build/CMakeFiles/CMakeOutput.log`
- `build/CMakeFiles/CMakeError.log`
- The exact `cmake` configure command (or preset + extra flags)
- `ctest --test-dir ./build -V` output for the failing test

