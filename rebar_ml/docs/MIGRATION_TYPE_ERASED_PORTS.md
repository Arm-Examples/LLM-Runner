<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Migration Guide: Move rebar-ml core to type-erased ports/payloads

This guide is a step-by-step plan for implementing the RFC:

- `rebar_ml/docs/RFC_TYPE_ERASED_PORTS.md`

The end goal is a **media-agnostic** core runtime (routing/graph/session code),
while keeping **typed processing** inside node/model runtimes.

## What “done” looks like

After this migration:

- Adding a new payload type (example: `TensorDict`, embeddings, video frames) does
  **not** require edits across core runtime plumbing (`NodeImpl`, `PipelineImpl`,
  `NodeSessionImpl`).
- Core only needs to understand:
  - `TypeId` (stable payload identity)
  - `AnyPayload` (type-erased storage)
  - generic port connect/disconnect + routing
- The public C++ API no longer has modality-specific methods (`get_text_*`,
  `get_audio_*`, ...). Instead it exposes generic, type-erased entry points:
  - `INode::{get_input_port,get_output_port}(TypeId, index)`
  - `INodeSession::{get_input,get_output}(TypeId, index)`
  plus templated convenience wrappers (`get_input<T>`, `get_output<T>`, etc).
  Java/JNI can remain typed as a thin layer over these primitives.

## Review strategy (recommended PR boundaries)

Treat this as a refactor series. Each PR should:

- keep the build green,
- include a focused doc update (this file + small `ARCH.md` notes),
- include a test that demonstrates the new slice “in action”.

Below is a suggested sequence.

## PR 1: Add internal type-erasure primitives (no behaviour change)

Goal: introduce `TypeId` + `AnyPayload` *internally* without changing runtime
behaviour or the public API.

Suggested changes:

- Add internal headers under:
  - `rebar_ml/core/src/cpp/src/core/runtime/` (example: `type_id.hpp`, `any_payload.hpp`)
- Provide helpers:
  - `TypeId type_id_of<TextData>()` and friends for built-ins.
  - `AnyPayload make_payload(T)` and `const T& payload_as<T>(AnyPayload)` with
    a `TypeId` check.

Tests:

- Add a small unit test under `rebar_ml/test/cpp/` validating:
  - `TypeId` stability for built-ins,
  - payload wrapping/unwrapping works and rejects mismatched types.

Doc update:

- Add a short “Planned: type-erased core I/O” pointer in `rebar_ml/docs/ARCH.md`
  linking to the RFC.

## PR 2: Introduce “AnyPort” internals and refactor `NodeImpl` storage

Goal: eliminate `text_inputs_` / `audio_inputs_` / `image_inputs_` vectors inside
`NodeImpl` and replace them with a generic port registry.

Files touched (core hotspots):

- `rebar_ml/core/src/cpp/src/core/runtime/node_impl.hpp`
- `rebar_ml/core/src/cpp/src/core/runtime/node_impl.cpp`
- `rebar_ml/core/src/cpp/src/core/runtime/ports.hpp`

Suggested approach:

- Add internal base interfaces:
  - `InputPortAnyInternal` / `OutputPortAnyInternal`
  - expose `TypeId type_id() const` and upstream/downstream accessors.
- Store ports in a generic registry:
  - per-direction `std::vector<std::shared_ptr<...>>` and a lookup map keyed by
    `(TypeId, index)`, or
  - a per-direction `unordered_map<TypeId, std::vector<...>>`.
- Keep typed APIs working by delegating:
  - `get_input_port(type_id_of<TextData>(), i)` becomes `return port_registry.get(type_id_of<TextData>(), i);`

Tests:

- Existing tests (`rebar_ml/test/cpp/RebarTest.cpp`) should stay green if typed APIs
  remain stable.
- Add a new test covering neighbor enumeration/topological checks using mixed
  modality ports without needing modality-specific loops.

## PR 3: Unify pipeline delivery/forwarding and terminal queues

Goal: remove `deliver_text/audio/image` and `forward_text/audio/image` from the
pipeline runtime, replacing them with a single type-erased path.

Files touched:

- `rebar_ml/core/src/cpp/src/core/runtime/pipeline_session_impl.hpp`
- `rebar_ml/core/src/cpp/src/core/runtime/pipeline_session_impl.cpp`

Suggested approach:

- Replace:
  - `TerminalQueue<TextData>` / `TerminalQueue<AudioData>` / `TerminalQueue<ImageData>`
  - `text_queues_` / `audio_queues_` / `image_queues_`
  - with a single `TerminalQueueAny` keyed by `(node, output_index, TypeId)`.
- Update typed `OutputStreamImpl<T>` to read from `TerminalQueueAny` and convert
  `AnyOutputItem` → `OutputItem<T>` with `TypeId` validation.

Tests:

- Extend `rebar_ml/test/cpp/RebarTest.cpp` to validate `final=false` streaming still
  works for an existing node (or add a small test node that emits partial items).

## PR 4: Unify node session dispatch (inputs/outputs)

Goal: remove modality-specific dispatch branching in `NodeSessionImpl` and unify
stream storage.

Files touched:

- `rebar_ml/core/src/cpp/src/core/runtime/node_session_impl.cpp`
- `rebar_ml/core/src/cpp/src/core/runtime/node_session_impl.hpp`

Suggested approach:

- Replace `if constexpr` dispatch (`deliver_text` / `deliver_audio` / `deliver_image`)
  with:
  - `pipeline->deliver(node, type_id_of<T>(), index, make_payload(std::move(value)))`
- Replace separate `text_inputs_` / `audio_inputs_` / `image_inputs_` containers
  with a generic registry keyed by `(TypeId, index)`.

Tests:

- Existing smoketests should remain stable.
- Add a concurrency regression test (debug-only if needed) verifying the existing
  single-producer assertion behaviour continues to hold.

## PR 5: Replace `NodeRuntime::{on_text,on_audio,on_image}` with a handler registry

Goal: make the runtime-to-node boundary generic so adding new payload types does
not require editing `NodeRuntime`.

Files touched:

- `rebar_ml/core/src/cpp/src/core/runtime/node_impl.hpp` (NodeRuntime)
- Node runtime implementations under `rebar_ml/models/**` (adapt to new handler shape)

Suggested approach:

- Introduce `NodeRuntime::on_any(TypeId, size_t, AnyPayload)`.
- Provide a helper base class for typed node authors:
  - `register_handler<TextData>(...)` stores a lambda in a map keyed by `TypeId`.
- Keep temporary compatibility shims:
  - old `on_text(...)` overrides can be adapted to call a default typed handler,
    to avoid converting every node in one PR.

Tests:

- Add a focused unit test node in `rebar_ml/test/cpp/` that:
  - registers a handler for a custom payload type,
  - forwards it through the pipeline,
  - proves that core routing did not need a new modality-specific code path.

## PR 6: Expose a generic port/stream API publicly

To avoid a closed-set API in the public C++ layer, expose generic public APIs:

- `INode::{get_input_port,get_output_port}(TypeId, index)`
- `INodeSession::{get_input,get_output}(TypeId, index)`
- `IInputStreamAny` / `IOutputStreamAny`

Typed access stays available via templated wrappers (`get_input<T>`, `get_output<T>`,
`get_input_port<T>`, `get_output_port<T>`).

## PR 7: Cleanup + deletion pass

After the generic path is the only path:

- delete modality-specific internal maps/vectors/loops,
- remove `Text/Audio/Image` branching from the core runtime,
- keep typed wrappers only at API edges (if desired).

This should be a mechanical PR with strong test coverage already in place.

## “Proof of success” acceptance test

Add a synthetic payload type in tests (or a real `TensorDict` payload behind
`USE_CF`) to demonstrate:

- new payload type wired through ports,
- no changes required in:
  - `PipelineImpl`,
  - `NodeSessionImpl`,
  - `NodeImpl` internals.

That acceptance test becomes the guardrail preventing regression back into a
closed set.
