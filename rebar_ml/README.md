<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# rebar-ml

rebar-ml is built as part of this repository by default. Disable it with `-DBUILD_REBAR=OFF`.

This directory currently hosts a small C++ dataflow graph runtime (nodes + ports + pipeline sessions) with
an optional Java/JNI wrapper. The C++ API lives under the `rebar_ml::` namespace, and the Java surface lives
under `com.arm.rebarml`.

## Build

```sh
cmake --preset=native -B build
cmake --build ./build
ctest --test-dir ./build --output-on-failure
```

## Compute Flow (optional, `USE_CF`)

rebar-ml can optionally link against a Compute Flow “Application SDK” to enable CF-backed nodes such as
`librebar_ml_model_cf_moonshine_stt`.

When the Compute Flow wiring is present (`rebar_ml/cf/`), it is enabled by default. Disable it with
`-DUSE_CF=OFF`.

Build with Compute Flow enabled (explicit):

```sh
cmake --preset=native -B build -DUSE_CF=ON
cmake --build ./build
```

When `USE_CF=ON`, CMake requires a Compute Flow SDK (`COMPUTE_FLOW_SDK_DIR`) and can auto-provision
one during configure.

### CF smoketests

The CF Moonshine smoketest is skipped unless `REBAR_CF_MOONSHINE_PACKAGE_DIR` is set to the installed
Moonshine model package directory (contains `moonshine_manifest_base_en.json`):

```sh
export REBAR_CF_MOONSHINE_PACKAGE_DIR=/path/to/moonshine-base-en
ctest --test-dir ./build -R rebar_ml --output-on-failure
```

## Outputs

- Core native shared library: `librebar_ml_core` (in the build `lib/` output directory)
- Model native shared libraries (built when `BUILD_REBAR=ON`):
  - `librebar_ml_model_wavtopcm`
  - `librebar_ml_model_pcms16letof32le`
  - `librebar_ml_model_moonshine_stt`
  - `librebar_ml_model_cf_moonshine_stt` (only when `USE_CF=ON`)
  - `librebar_ml_model_mnn_omni`
- Core JNI shared library: `librebar_ml_jni_core` (in the build `lib/` output directory, only when `BUILD_JNI_LIB=ON`)
- Model JNI shared libraries (built when `BUILD_JNI_LIB=ON`):
  - `librebar_ml_jni_model_wavtopcm`
  - `librebar_ml_jni_model_pcms16letof32le`
  - `librebar_ml_jni_model_moonshine_stt`
  - `librebar_ml_jni_model_cf_moonshine_stt` (only when `USE_CF=ON`)
  - `librebar_ml_jni_model_mnn_omni`
- Core Java archive: `rebar_ml.jar` (in the build `lib/` output directory, only when `BUILD_JNI_LIB=ON`)
- Model Java archives (optional convenience wrappers, built when `BUILD_JNI_LIB=ON`):
  - `rebar_ml_model_wavtopcm.jar`
  - `rebar_ml_model_pcms16letof32le.jar`
  - `rebar_ml_model_moonshine_stt.jar`
  - `rebar_ml_model_cf_moonshine_stt.jar` (only when `USE_CF=ON`)
  - `rebar_ml_model_mnn_omni.jar`

## Notes

- The default build enables working audio utility nodes (`wavtopcm`, `pcms16letof32le`).
- The non-CF `moonshine_stt` and `mnn_omni` backends are stubbed in this repo.
- When built with `USE_CF=ON` and a provisioned Compute Flow SDK, `cf_moonshine_stt` provides a
  functional Moonshine STT implementation backed by Compute Flow.
- Shared libraries use the platform extension (for example `.so` on Linux and `.dylib` on macOS).
