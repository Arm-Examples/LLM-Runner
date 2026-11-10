<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# RFC: Type-erased ports/payloads in rebar-ml core (media-agnostic runtime)

<!-- TOC -->
* [RFC: Type-erased ports/payloads in rebar-ml core (media-agnostic runtime)](#rfc-type-erased-portspayloads-in-rebar-ml-core-media-agnostic-runtime)
  * [Motivation](#motivation)
  * [Goals / non-goals](#goals--non-goals)
  * [Background: current “closed set of modalities”](#background-current-closed-set-of-modalities)
  * [Proposal: type-erased core I/O](#proposal-type-erased-core-io)
    * [New primitives](#new-primitives)
    * [Port model](#port-model)
    * [Runtime delivery/forwarding](#runtime-deliveryforwarding)
    * [Node runtime handlers (typed processing stays in nodes/models)](#node-runtime-handlers-typed-processing-stays-in-nodesmodels)
    * [JNI/Java considerations](#jnijava-considerations)
  * [Compatibility strategy](#compatibility-strategy)
  * [Relationship to Compute Flow and future remote providers](#relationship-to-compute-flow-and-future-remote-providers)
  * [Alternatives considered](#alternatives-considered)
  * [Risks / open questions](#risks--open-questions)
<!-- /TOC -->

## Motivation

Today, rebar-ml’s C++ core/runtime has a **closed set** of modalities: `TextData`,
`AudioData`, `ImageData`.

That choice makes the public API and the Java/JNI surface easy to understand, but
it also means that adding *any* new payload type (for example: video frames,
embeddings, structured metadata, tensors) requires edits across a wide set of
core files (node topology, pipeline runtime, node session dispatch, output queue
plumbing, etc).

This RFC proposes refactoring the **core routing/graph runtime** to be
**modality-agnostic** by switching internal delivery/forwarding to **type-erased
payloads** identified by a stable `TypeId`. Typed processing remains in
node/model runtimes.

## Goals / non-goals

Goals:

- Make core graph/runtime logic **independent of specific modalities**.
- Make adding a new payload type require changes only in:
  - the node/model implementation(s) that use it (and optionally API wrappers),
  - not in core runtime routing and topology logic.
- Preserve the current typed C++ and Java APIs (Text/Audio/Image) as **thin
  wrappers** initially, to avoid breaking existing callers.
- Use a **stable** type identifier (not RTTI / `typeid(T)`) so the type system
  can extend to:
  - JNI boundaries,
  - dynamic loading boundaries,
  - future remote execution / transport layers.

Non-goals (this RFC does not try to solve these):

- Change the actor/threading model (see `ARCH.md` and `MIGRATION_GUIDE.md`).
- Define per-edge buffering/backpressure policies (implemented separately via
  `ConnectionOptions` and a tee/fan-out node; not required by type erasure).
- Define a universal serialization format for remote execution (that becomes
  relevant once transport is introduced).

## Background: historical “closed set of modalities” (pre-migration)

Historically, the “closed set” (Text/Audio/Image) was present in both API and internal runtime plumbing.

- Public API used to be modality-specific; the target shape (now implemented) is
  type-erased:
  - `rebar_ml/core/src/cpp/include/rebar/rebar.hpp`:
    - `INode::{get_input_port,get_output_port}(TypeId, index)` + templated wrappers
      (`get_input_port<T>(index)`, `get_output_port<T>(index)`).
    - `INodeSession::{get_input,get_output}(TypeId, index)` + templated wrappers
      (`get_input<T>(index)`, `get_output<T>(index)`).
- Historically, `NodeImpl` stored and enumerated separate modality-specific port vectors:
  - `rebar_ml/core/src/cpp/src/core/runtime/node_impl.hpp`
- Historically, `PipelineImpl` had separate deliver/forward paths + terminal queues:
  - `rebar_ml/core/src/cpp/src/core/runtime/pipeline_session_impl.hpp`
- Historically, `NodeRuntime` was modality-specific (`on_text`, `on_audio`, `on_image`):
  - `rebar_ml/core/src/cpp/src/core/runtime/node_impl.hpp`
- Historically, `NodeSessionImpl::InputStreamImpl::dispatch` branched by `T`:
  - `rebar_ml/core/src/cpp/src/core/runtime/node_session_impl.cpp`

This creates an expanding “edit surface” as modalities are added.

## Proposal: type-erased core I/O

### New primitives

Introduce a small set of primitives to represent “a payload of some type”:

- `TypeId`: a stable identifier for a payload type.
  - Recommended representation:
    - canonical string name, e.g. `rebar_ml.text.v1`, `rebar_ml.audio.v1`
    - plus an optional pre-hashed `uint64_t` for fast lookups.
  - Important: avoid `std::type_index` / `typeid(T)` as the primary identity.

- `AnyPayload` (type-erased payload envelope):
  - `TypeId type`
  - `std::shared_ptr<const void> storage` **or** a small virtual base
    (`AnyPayloadBase`) with:
    - `TypeId type() const`
    - typed accessors implemented by a templated derived class.
  - Must support copy/move and cross-thread ownership safely.

- `AnyOutputItem`:
  - `AnyPayload payload`
  - `bool final`

Notes:

- For existing built-in types, `TypeId` is fixed:
  - `TextData` → `rebar.text.v1`
  - `AudioData` → `rebar.audio.v1` *(codec/sample-rate/channels remain inside `AudioData`)*
  - `ImageData` → `rebar.image.v1`
- New types (examples):
  - `rebar.tensor_dict.v1` (for CF-style tensor payloads)
  - `rebar.embedding.f32.v1` (vector<float> + metadata)
  - `rebar.video.frame.v1` (encoded or raw frames)

### Port model

Internally, represent ports generically:

- Replace `NodeImpl::{text,audio,image}_{inputs,outputs}_` with:
  - `std::vector<std::shared_ptr<InputPortAny>> inputs_`
  - `std::vector<std::shared_ptr<OutputPortAny>> outputs_`
  - and/or a registry indexed by `(TypeId, index)` per direction.

Each port carries a `TypeId` describing what payload it accepts/emits:

- `InputPortAny { TypeId type; size_t index; ... }`
- `OutputPortAny { TypeId type; size_t index; ... }`

Connection validation becomes:

- `OutputPortAny.type == InputPortAny.type` must hold (same as today, but without
  hardcoding Text/Audio/Image).

Typed ports remain as wrappers:

- `IInputPort<TextData>` / `IOutputPort<AudioData>` stay in place for now.
- They become thin adapters over `InputPortAny` / `OutputPortAny` plus a checked
  cast using `TypeIdOf<T>`.

This immediately eliminates code patterns like:

```cpp
for (const auto& port : text_inputs_) ...
for (const auto& port : audio_inputs_) ...
for (const auto& port : image_inputs_) ...
```

Core neighbor enumeration can instead iterate a single list of input/output
ports.

### Runtime delivery/forwarding

Replace modality-specific pipeline methods with a single generic path:

- Old:
  - `deliver_text(...)`, `deliver_audio(...)`, `deliver_image(...)`
  - `forward_text(...)`, `forward_audio(...)`, `forward_image(...)`
- New:
  - `deliver_payload(NodeImpl* node, TypeId type, size_t input_index, AnyPayload payload)`
  - `forward_payload(NodeImpl* node, TypeId type, size_t output_index, AnyPayload payload, bool final)`
  - (optional) small templated helpers: `deliver<T>(...)` / `forward<T>(...)`

Terminal output queues also become generic:

- Old: `TerminalQueue<TextData>` / `TerminalQueue<AudioData>` / ...
- New: `TerminalQueue<AnyPayload>` keyed by `(node, TypeId, output_index)`.

Typed output streams (`IOutputStream<TextData>`) are wrappers that:

- call `TerminalQueue<AnyPayload>::receive()`,
- validate `TypeId`,
- decode to `OutputItem<T>`.

Threading behaviour remains the same:

- `send()`/`submit()` enqueue a task onto the destination node’s actor thread.
- The actor thread calls into `PipelineImpl::deliver_payload(...)`.

### Node runtime handlers (typed processing stays in nodes/models)

We want:

- a modality-agnostic core runtime, **and**
- a pleasant typed authoring experience for node implementations.

Proposed shape:

- `NodeRuntime` becomes an internal dispatcher:
  - `Result<void> on_any(TypeId type, size_t port, AnyPayload payload)`
  - optionally `finish()`, `cancel()` unchanged.

Typed node runtimes register handlers:

- `register_handler<TextData>([](size_t port, TextData data) { ... })`
- or implement `on_any(...)` and use helper `payload_as<TextData>(payload)`

This keeps all modality awareness at node/model boundaries.

### JNI/Java considerations

Java currently exposes a typed API (`TextInput`, `AudioInput`, etc) and that is
useful to keep stable.

This RFC recommends:

- Keep the Java surface typed initially.
- Internally, have JNI pass a `TypeId` (or an enum mapping to `TypeId`) into the
  generic core API.
- If we later expose a generic Java port API, prefer:
  - string `TypeId` values (stable), rather than Java enum ordinals.

## Compatibility strategy

The minimal-risk migration path:

1. Introduce type-erased plumbing internally (no public API changes).
2. Make existing typed APIs delegate to the generic core.
3. Only after the internal refactor stabilizes, consider adding a public generic
   API (optional) and deprecating typed-only accessors in the C++ surface.

This approach keeps:

- existing examples/tests working,
- Android/JNI stable,
- reviewers able to validate behaviour at each step.

## Relationship to Compute Flow and future remote providers

Compute Flow (CF) already models I/O as a type-erased container (`TensorDict`).

Benefits of this RFC for CF integration:

- A CF-backed node can emit/receive richer payload types (tensors/structured)
  without forcing rebar-ml core changes for every new “modality”.
- A future `TensorDict` payload type can be represented as just another `TypeId`
  (`rebar.tensor_dict.v1`) with node-local interpretation.

Benefits for remote providers (gRPC or otherwise):

- A stable `TypeId` is the natural dispatch key for transport messages.
- The runtime can route locally or remotely without “Text/Audio/Image” being
  baked into the core scheduler/graph semantics.

## Alternatives considered

- **Keep adding modalities as first-class types**:
  - Simple short-term, but long-term high maintenance and high review overhead.
- **Use a `std::variant<TextData, AudioData, ImageData, ...>` in core**:
  - Still a closed set; needs edits everywhere when a new type is added.
- **Template the core on a compile-time type list**:
  - Still a closed set per build; complicates ABI, JNI, and transport.
- **Push all modality abstraction into Java/JNI only**:
  - Still leaves the C++ core closed-set and does not help native-only users.

## Risks / open questions

- **Exact `AnyPayload` representation**:
  - Choose a design that is safe across shared libraries and does not rely on
    RTTI identity for correctness.
- **TypeId governance**:
  - Decide naming conventions and versioning rules (`.v1`), and where constants
    live (core header vs internal registry).
- **Performance/allocations**:
  - Type-erased wrappers add some indirection; likely negligible relative to ML
    inference, but avoid pathological per-item heap churn for high-rate streams.
- **Public API direction**:
  - Decide whether the long-term goal is:
    - typed-first with generic as “advanced”, or
    - generic-first with typed as convenience wrappers.

See `MIGRATION_TYPE_ERASED_PORTS.md` for a concrete step-by-step refactor plan.
