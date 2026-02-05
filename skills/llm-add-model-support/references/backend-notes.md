<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Backend notes for model onboarding

## `llama.cpp`

- Typically uses `.gguf` models.
- Prompt formatting and stop tokens matter; validate that `LLM::NextToken()` stop token behavior matches the model’s expected stop words.
- For AArch64 feature tuning, `CPU_ARCH` can matter (linux-aarch64 + llama.cpp only).

## `onnxruntime-genai`

- Models are often directory-based bundles; ensure config points at the right folder structure.
- If you change model inputs/outputs, keep benchmark and tests in sync.

## `mediapipe` (experimental)

- Treat as Android-focused and experimental; document any constraints explicitly.
- Models may have stricter asset/layout expectations; keep paths deterministic.

## `mnn`

- Models are often referenced by `config.json` within a model directory.
- KleidiAI may not fully enable at runtime; avoid claiming optimizations unless verified.

