<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# What `llm_doctor.py` checks

- System info: OS/arch, Python version, CMake version, git HEAD
- Build dir presence: `CMakeCache.txt`, `CMakeOutput.log`, `CMakeError.log`
- Cache highlights (if present): `LLM_FRAMEWORK`, `BUILD_JNI_LIB`, `BUILD_LLM_TESTING`, `BUILD_BENCHMARK`, `USE_KLEIDIAI`, `CMAKE_BUILD_TYPE`
- Tooling (when JNI enabled): `java`, `javac`, `JAVA_HOME`
- Test wiring sanity: config filenames referenced in `test/CMakeLists.txt` exist in `model_configuration_files/` and parse as JSON
- Artifacts (best-effort): `build/bin/llm-cpp-tests`, `build/lib/libllm-jni.so` (when JNI enabled)

It is intentionally “fast”: it does not rebuild, run tests, or re-download models.

