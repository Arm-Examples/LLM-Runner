<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Output validation & test alignment (new models)

This repo’s tests currently validate outputs using substring checks on a few “anchor” facts (e.g., `"Paris"` for a capital-city question, or `"tiger"` for an image label). New models can change phrasing, casing, or add extra text; update tests without making them meaningless.

## Preferred strategy (in order)

1) **Constrain the prompt** to reduce variation
- “Answer with a single word.”
- “Reply with only the country name.”
- “Describe in 1 short sentence.”

2) **Check for a stable anchor**
- Names/entities are better than full sentences.
- Accept a small set of alternatives when real (e.g., `["dog", "puppy"]` already used in `test/cpp/LlmTest.cpp`).

3) **Normalize before checking (when needed)**
- Use case-insensitive matching by lowercasing the response and expected tokens.
- Avoid punctuation/whitespace sensitivity.

4) **Only then broaden acceptance**
- Regex/keyword sets can be OK, but keep them tight.
- Avoid “non-empty output” checks unless you’re explicitly testing liveness/progress.

## When it is OK to revise expected outputs

- The new model is correct but uses a different valid synonym or casing.
- The prompt/template changes for a backend (chat templates, stop words) change the output shape.
- The model adds harmless boilerplate (e.g., “The capital is Paris.”) and your assertion was too strict.

## When it is NOT OK to revise expected outputs

- The model is wrong (hallucination, incorrect entity).
- The model is failing to follow modality constraints (vision model not recognizing obvious objects).
- The output indicates it didn’t load the model correctly (empty, repeated tokens, errors).

## Practical “proof” loop for a new model

- Add config + model assets.
- Run the config-specific ctest (find it via `ctest --test-dir ./build -N` then run with `-R <name> -V`).
- If failures are output-drift only:
  - tighten prompts and/or extend the small expected-token whitelist
  - rerun and ensure the test still fails under an intentionally broken condition (wrong model path, wrong image, etc.)

