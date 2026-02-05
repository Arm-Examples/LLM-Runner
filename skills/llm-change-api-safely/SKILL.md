---
name: llm-change-api-safely
description: Make safe changes to the public C++ API (src/cpp/interface/) and its implementations (C++ + optional JNI), including updating tests and preserving compatibility where possible. Use when adding new features/methods, changing behavior, or refactoring interface/bridge code in this repository.
---

# LLM change API safely

The public C++ surface is `LLM` in `src/cpp/interface/Llm.hpp`, implemented in `src/cpp/Llm.cpp` and backed by framework-specific implementations under `src/cpp/frameworks/`. JNI bindings (if enabled) live under `src/cpp/LlmJni.cpp` and `src/java/`.

## Workflow

### 1) Identify affected layers

- **C++ API surface**: `src/cpp/interface/`
- **Core implementation / glue**: `src/cpp/Llm.cpp`, `src/cpp/LlmBridge.cpp`
- **Config**: `src/cpp/config/`
- **Framework implementations**: `src/cpp/frameworks/`
- **JNI** (if enabled): `src/cpp/LlmJni.cpp`, `src/java/`
- **Tests**: `test/cpp/`

Load the checklist reference: `skills/llm-change-api-safely/references/api-change-checklist.md`.

### 2) Prefer additive, backwards-compatible changes

- Prefer adding new methods/flags over changing semantics of existing methods.
- If behavior changes are required, update tests to lock the intended behavior.

### 3) Update C++ and (optionally) JNI in lockstep

- Implement the C++ behavior first and add/adjust tests under `test/cpp/`.
- If JNI is enabled by default for your preset, ensure the Java-facing behavior remains consistent or update it accordingly.

### 4) Prove it works

```sh
cmake --preset=native -B build
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

To isolate C++ changes from Java/JNI issues:

```sh
cmake --preset=native -B build -DBUILD_JNI_LIB=OFF
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```
