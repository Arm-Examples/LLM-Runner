<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# rebar-ml migration scaffold: Examples 1 and 3 (formerly “Media Stream”)

This directory contains migration notes for the earlier “Media Stream” API and
its Example 1 / Example 3 flows, updated to match this repository’s current
rebar-ml API, layout, and naming.

Paths in this document are shown relative to the repo root (the directory
containing `CMakePresets.json`).

See also:

- Architecture: `ARCH.md`
- Android integration: `ANDROID_README.md`
- Detailed migration notes: `MIGRATION_GUIDE.md`
- RFC: type-erased ports/payloads (media-agnostic core): `RFC_TYPE_ERASED_PORTS.md`
- Migration plan: type-erased ports/payloads: `MIGRATION_TYPE_ERASED_PORTS.md`
- Compute Flow integration design: `COMPUTE_FLOW_INTEGRATION.md`
- CF I/O adapter design: `CF_IO_ADAPTER.md`
- Review plan / PR breakdown: `REVIEW_PLAN.md`
- Test/build verification: `VERIFICATION.md`

rebar-ml is a small graph runtime:

- Nodes (`rebar_ml::INode` / `com.arm.rebarml.RebarNode`) own reusable topology
  (ports + connections).
- Pipeline sessions (`rebar_ml::IPipelineSession` / `RebarPipelineSession`) are
  isolated runtime instances and own one node session per node
  (`rebar_ml::INodeSession` / `RebarNodeSession`).

## Example 1

```text
App WAV ByteBuffer
  -> Java/JNI AudioInput
  -> C++ wav-to-pcm node (rebar_ml::INode)
  -> C++ pcm-s16le-to-f32le node (rebar_ml::INode)
  -> native PCM connection (ports)
  -> C++ cf_moonshine_stt node (rebar_ml::INode, Compute Flow-backed)
  -> Java/JNI TextOutput
  -> App transcript
```

## Example 3

```text
App PCM ByteBuffer
  -> Java/JNI AudioInput
  -> C++ moonshine_stt node (rebar_ml::INode)
  -> native recognized-text connection (ports)
  -> C++ mnn_omni node (rebar_ml::INode)
       -> Java/JNI TextOutput -> App text
       -> Java/JNI AudioOutput -> App audio
```

## Important: some backends are migration stubs

The generic runtime + C++/JNI/Java plumbing are implemented. These integration
points are stubs by default and must be replaced in product builds:

1. STT backend factory (non-CF Moonshine node): `rebar_ml/models/stt/moonshine/src/cpp/moonshine_backend.cpp`
2. Omni backend factory: `rebar_ml/models/llm-framework/mnn-omni/src/cpp/application_omni_backend.cpp`
3. Output buffer ownership (copy-free path): see `MIGRATION TODO` in:
   - `rebar_ml/core/src/cpp/src/core/jni/rebar_jni_core.cpp`
   - `rebar_ml/core/src/java/com/arm/rebarml/RebarBuffer.java`

Compute Flow-backed nodes under `rebar_ml/models/cf/...` are functional when built with `-DUSE_CF=ON`
and when the required brick + model packages are installed.

See `MIGRATION_GUIDE.md` for details.

## Source layout (rebar-ml)

```text
rebar_ml/core/src/cpp/include/rebar/         Public C++ API (nodes, ports, sessions)
rebar_ml/core/src/cpp/src/core/runtime/      Generic runtime implementation
rebar_ml/core/src/cpp/src/core/jni/          JNI bridge for com.arm.rebarml
rebar_ml/core/src/java/com/arm/rebarml/        Java API facade
rebar_ml/cf/                                 Compute Flow SDK build wiring (optional, USE_CF)
rebar_ml/models/utils/wavtopcm/              WAV -> PCM node
rebar_ml/models/utils/pcms16letof32le/       PCM S16LE -> PCM F32LE node
rebar_ml/models/stt/moonshine/               STT node + backend factory hook
rebar_ml/models/cf/stt/moonshine/            Compute Flow-backed Moonshine STT node (USE_CF)
rebar_ml/models/llm-framework/mnn-omni/      Omni node + backend factory hook
rebar_ml/examples/                           Small C++ examples (e.g. WavToStt)
rebar_ml/test/                               C++ tests + optional JNI tests
```

## Build and verify on a host

Build from the repo root (the directory containing `CMakePresets.json`).
rebar-ml is enabled by default:

```sh
cmake --preset=native -B build
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

Disable rebar-ml with `-DBUILD_REBAR=OFF`.

To build the rebar-ml example(s), add `-DBUILD_REBAR_EXAMPLES=ON`.

## Android integration

See `ANDROID_README.md`.
