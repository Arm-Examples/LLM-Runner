<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# rebar-ml architecture

<!-- TOC -->
* [rebar-ml architecture](#rebar-ml-architecture)
* [Core concepts](#core-concepts)
  * [Topology vs runtime](#topology-vs-runtime)
  * [Data model](#data-model)
  * [Ports and connections](#ports-and-connections)
  * [Pipeline sessions](#pipeline-sessions)
  * [Input staging: `add()` + `submit()` vs `send()`](#input-staging-add--submit-vs-send)
  * [Outputs and end-of-stream](#outputs-and-end-of-stream)
* [Execution model](#execution-model)
* [Extending rebar-ml](#extending-rebar-ml)
* [Known stubs / migration seams](#known-stubs--migration-seams)
* [Android integration](#android-integration)
<!-- /TOC -->

rebar-ml is a small, backend-agnostic graph runtime built as an optional component
of this repo. It provides:

- A thin C++ API (`rebar_ml::INode`, `rebar_ml::IPipelineSession`, typed ports/streams).
- An optional Java/JNI surface (`com.arm.rebarml.*`) for Android/Java apps.

This document describes the current rebar-ml API and its runtime behaviour.

## Core concepts

### Topology vs runtime

rebar-ml separates *topology* (reusable graph wiring) from *runtime state* (a single
execution instance):

- **Node / topology (reusable)**
  - C++: `rebar_ml::INode`
  - Java: `com.arm.rebarml.RebarNode`
  - Owns ports and connections.

- **Pipeline session (runtime instance)**
  - C++: `rebar_ml::IPipelineSession`
  - Java: `com.arm.rebarml.RebarPipelineSession`
  - Represents one isolated execution of the connected topology (per-user, per-request,
    per-call, etc).

- **Node session (runtime per node per pipeline)**
  - C++: `rebar_ml::INodeSession`
  - Java: `com.arm.rebarml.RebarNodeSession`
  - Provides typed input/output streams for one node instance inside one pipeline.

Topology can be reused to create multiple independent pipeline sessions, but a
topology cannot be modified while any pipeline session is alive.

### Data model

rebar-ml supports three modalities as separate typed APIs:

- Text: `rebar_ml::TextData` / `com.arm.rebarml.TextData`
- Audio: `rebar_ml::AudioData` / `com.arm.rebarml.AudioData`
- Image: `rebar_ml::ImageData` (C++ only in this repo today)

Note: the current core/runtime implementation is also modality-aware (it has
explicit Text/Audio/Image routing paths). The planned direction is to make the
core runtime **modality-agnostic** by routing type-erased payloads identified by
a stable `TypeId`, while keeping typed processing in node/model runtimes. See:

- RFC: `RFC_TYPE_ERASED_PORTS.md`
- Migration plan: `MIGRATION_TYPE_ERASED_PORTS.md`

Audio is carried as an opaque byte buffer plus an `AudioFormat`:

- `AudioCodec::pcm_s16le` / `AudioCodec.PCM_S16LE`: interleaved little-endian signed 16-bit PCM.
- `AudioCodec::pcm_f32le` / `AudioCodec.PCM_F32LE`: interleaved little-endian float32 PCM.
- `AudioCodec::wav` / `AudioCodec.WAV`: WAV container bytes (typically 16-bit PCM WAV in this repo).

Outputs use `OutputItem<T>` with a `final` flag to support streaming-style
delivery (intermediate vs final chunks).

### Ports and connections

Nodes expose typed ports (examples):

- C++: `IAudioOutputPort` → `IAudioInputPort`
- Java: `AudioOutputPort` → `AudioInputPort`

Connection rules:

- **One-to-one**: an output port can connect to at most one destination, and an
  input port can have at most one upstream connection.
- **No topology edits while running**: `connect()` / `disconnect()` fail with
  `graph_in_use` if any pipeline session exists for either endpoint node.
- **ConnectionOptions**: each connection has a queue capacity and overflow
  policy (block/fail/drop).

Fan-out:

- Because output ports are one-to-one, you need a dedicated node to “tee”
  (duplicate) a stream to multiple consumers.
- rebar-ml includes a simple built-in tee node:
  - C++: `#include "rebar/tee.hpp"` and `rebar_ml::make_tee_node(rebar_ml::TeeOptions{.output_count = N})`
  - Java: `com.arm.rebarml.Tee.createNode(N)`

### Pipeline sessions

A pipeline session is created from any node in a connected topology:

- C++: `node->create_pipeline_session()`
- Java: `node.createPipelineSession()`

The session discovers the full connected graph (upstream + downstream),
validates it (including cycle detection), and creates per-node runtimes.

### Input staging: `add()` + `submit()` vs `send()`

Inputs are written through typed input streams returned from a node session:

- C++: `rebar_ml::IAudioInput`, `rebar_ml::ITextInput`, ...
- Java: `AudioInput`, ...

There are two ways to submit input:

1. **Stage** one or more items with `add(...)`, then flush all staged items for
   the pipeline with `pipeline_session->submit()` / `pipelineSession.submit()`.

2. **Send immediately** with `send(...)` when there is *no* staged submission
   pending.

Important behavioural constraints:

- `send()` fails with `input_pending` if any stream in the pipeline has staged
  data that has not been flushed via `submit()`.
- After all inputs are provided, call
  `finish_inputs()` / `finishInputs()` to flush any staged inputs and then
  finish the pipeline.

### Outputs and end-of-stream

Outputs are read from typed output streams:

- C++: `rebar_ml::ITextOutput::receive()` returns `OutputRetrieval<TextData>`.
- Java: `TextOutput.receive()` returns `ReceiveResult<TextData>`.

Receives block until one of:

- An item is available.
- The stream reaches end-of-stream (success with `null` item).
- The pipeline is cancelled.
- A node/runtime reports an error.

## Execution model

rebar-ml’s runtime is designed to keep node implementations simple:

- A pipeline session is *logically* single-owner (debug builds assert certain
  calls happen from the same thread).
- Node implementations are invoked by the runtime (see below); most nodes can
  be written without explicit locks as long as they follow the threading rules.

For application code this typically means:

- Construct topology on one thread.
- Create a pipeline session.
- Drive inputs (stage + submit, or send) and read outputs.
- Call `finish_inputs()` and then read until end-of-stream.
- Use `cancel()` or `teardown()` for shutdown/error handling.

### Threading and actor model (current implementation)

The runtime uses small per-node actors to decouple application threads and
connected nodes from node execution. Connected edges are delivered through a
bounded queue configured by `ConnectionOptions`.

Key points:

- **Topology building is single-threaded**: connect/disconnect ports before
  creating any pipeline session. Once a pipeline session exists, topology edits
  fail with `graph_in_use`.

- **Pipeline session calls are thread-confined**:
  - C++: methods like `get_node_session()`, `submit()`, and `finish_inputs()` are
    debug-asserted to run on the pipeline “owner thread”.
  - Java: treat `RebarPipelineSession` as thread-confined as well.

- **Nodes execute on small actor threads**:
  - Each node in a pipeline session owns a native actor thread that runs queued
    tasks in order.
  - Node sessions post work into that node’s actor thread.
  - Actor threads are stopped when the pipeline session is cancelled/torn down
    or destroyed. On Android, leaking a pipeline session can leak threads.

- **External inputs are asynchronous**:
  - `I*Input::send(...)` posts one delivery task to the destination node’s actor and
    returns.
  - `I*Input::add(...)` only stages data. `IPipelineSession::submit()` flushes
    staged data by posting delivery tasks to the relevant node actors.

- **Connected edges are queued and apply backpressure**:
  - When a node runtime forwards to a connected downstream port, the payload is
    enqueued into a per-connection bounded queue sized by
    `ConnectionOptions::queue_capacity`.
  - `ConnectionOptions::overflow` controls behaviour when the connection queue
    is full:
    - `block`: block the forwarding node’s actor thread until space is
      available (backpressure propagates upstream).
    - `fail`: fail the pipeline with `ErrorCode::queue_full`.
    - `drop_oldest` / `drop_newest`: drop items on the slow edge without
      failing the pipeline.
  - Delivery from the connection queue into the downstream node runs on the
    downstream node’s actor thread (not on the upstream node’s call stack).

- **Terminal outputs are queued**:
  - If an output port is *not* connected, the pipeline creates an internal
    terminal queue for that port.
  - `I*Output::receive()` / `TextOutput.receive()` blocks on that queue until an
    item, an error, cancellation, or end-of-stream is observed.

- **Finishing waits for the pipeline to become idle**:
  - `finish_inputs()` flushes any staged inputs and then waits for all pending
    deliveries (including queued connected-edge deliveries) to complete before
    running each node runtime’s `finish()` in topological order and closing
    terminal output queues.

### Thread-safety guidelines for applications

- Call `connect()`/`disconnect()` only during topology construction.
- Call pipeline-session methods (`getNodeSession`, `submit`, `finishInputs`,
  `cancel`, `teardown`, `close`) from one controlling thread.
- Treat each input stream as single-producer (debug builds assert this).
- Output `receive()` calls are safe to run on a background thread (they block).

## Extending rebar-ml

To add a new node type in C++:

1. Implement a `NodeImpl` subclass that:
   - Calls `add_input<T>()` / `add_output<T>()` to declare ports.
   - Implements `create_runtime(...)`.
2. Implement a `NodeRuntime` subclass. Most nodes in this repo derive from
   `BuiltInTypedNodeRuntime` (a small adapter in
   `rebar_ml/core/src/cpp/src/core/runtime/builtin_typed_node_runtime.hpp`) and
   override one or more of:
   - `on_text(...)`, `on_audio(...)`, `on_image(...)`
   - `finish()` and/or `cancel()`
3. Forward outputs using `pipeline.forward<T>(context, output_port_id, data, final)`.

### Adding a node: repo layout + build wiring

Recommended pattern (mirrors existing nodes in this repo):

1. Create a model directory under `rebar_ml/models/.../` with:
   - `include/rebar/models/<...>/<name>.hpp` (public factory + config structs)
   - `src/cpp/<name>_node.cpp` (node + runtime implementation)
   - `src/cpp/<name>_backend.cpp` (backend factory seam, if needed)
   - `CMakeLists.txt` that builds:
     - `librebar_ml_model_<name>.so` (C++ node)
     - optionally `librebar_ml_jni_model_<name>.so` + `rebar_ml_model_<name>.jar`

2. Add the new directory to `rebar_ml/models/CMakeLists.txt` so it builds when
   rebar-ml is enabled (`BUILD_REBAR=ON`).

3. Keep node APIs small and backend-neutral:
   - Put runtime graph concerns in the node/runtime (`NodeImpl` / `NodeRuntime`).
   - Put framework/model bindings behind a small backend interface (similar to
     `rebar_ml::ISttBackend` (defined in `rebar/models/stt/stt_backend.hpp`) /
     `rebar_ml::IOmniBackend` (defined in `rebar/models/llmframework/mnnomni/omni_backend.hpp`))
     so the node stays testable.

### Node sessions: what you do (and don’t) implement

You usually **do not** implement a custom node session type. `NodeSessionImpl`
is generic:

- If an input port exists and is **not connected**, a node session exposes an
  input stream for it (C++: `get_input<AudioData>(...)`, Java: `getAudioInput(...)`).
- If an output port exists and is **not connected**, a node session exposes an
  output stream for it (C++: `get_output<TextData>(...)`, Java: `getTextOutput(...)`).
- If a port is connected (input has an upstream, or output has a downstream),
  the corresponding stream is unavailable (`null` in Java).

This implies:

- To “tap” an intermediate output, you need to keep that output unconnected or
  introduce a dedicated fan-out/tee node (provided in this repo as
  `rebar/tee.hpp` / `com.arm.rebarml.Tee`).

### Exposing a node to Java/JNI (optional)

Follow the existing `wavtopcm`, `moonshine_stt`, or `mnn_omni` patterns:

1. Add a JNI library `rebar_ml_jni_model_<name>` that exports a `create*Node(...)`
   function returning a native `rebar_ml::INode` handle.
2. Add a small Java wrapper class (for example `com.arm.rebarml.models.<...>.<Name>`)
   that calls the JNI `create*Node(...)` and returns `RebarNode`.
3. Ensure the Java wrapper’s `NativeBindings` loads `rebar_ml_jni_model_<name>`.

If you need Java support for additional modalities (for example text input or
images), add new Java stream/port wrappers and JNI bindings by mirroring
existing classes like `AudioInput`, `TextOutput`, and their native methods.

## Known stubs / migration seams

Some model integrations in this repo are intentionally stubbed:

- STT backend factory: `rebar_ml/models/stt/moonshine/src/cpp/moonshine_backend.cpp`
- Omni backend factory: `rebar_ml/models/llm-framework/mnn-omni/src/cpp/application_omni_backend.cpp`
- Optional copy-free output: see `MIGRATION TODO` in:
  - `rebar_ml/core/src/cpp/src/core/jni/rebar_jni_core.cpp`
  - `rebar_ml/core/src/java/com/arm/rebarml/RebarBuffer.java`

See `MIGRATION_GUIDE.md` for integration guidance.

## Android integration

See `ANDROID_README.md` for a step-by-step Android integration guide (build
outputs, packaging, Gradle wiring, and runtime usage expectations).
