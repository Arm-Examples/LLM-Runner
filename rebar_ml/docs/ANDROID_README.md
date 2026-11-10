<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Android integration

This repo does not ship a ready-made Gradle module (AAR) for rebar-ml. The host
project must integrate the Java surface (sources or JARs) and package the
native libraries (`.so`).

This document is written to be “agent-friendly”: it lists the exact build
outputs and the minimal Android-side wiring needed for a clean integration.

Out of the box, the `wav-to-pcm` and `pcms16letof32le` model nodes are
functional. The non-Compute-Flow STT (`rebar_ml/models/stt/moonshine`) and Omni
(`mnn_omni`) backends are stubs in this repo and must be replaced for a
production integration (see `MIGRATION_GUIDE.md`).

If you enable Compute Flow (`-DUSE_CF=ON`) and install the `stt:moonshine` brick
and model package, you can use the CF-backed Moonshine STT node at
`rebar_ml/models/cf/stt/moonshine/` without replacing the stub backend.

## 1) Build Android artifacts

Run these commands from the repo root (the directory containing
`CMakePresets.json`).

Prerequisites:

- Android NDK 29.0.14206865+.
- The `x-android-aarch64` CMake preset requires `NDK_PATH` (env var) or
  `-DNDK_PATH=...`.

Example:

```sh
export NDK_PATH=/path/to/android-ndk

cmake --preset=x-android-aarch64 -B build-android -DBUILD_REBAR=ON
cmake --build ./build-android
```

Build outputs land under `build-android/lib/`:

- Core:
  - `build-android/lib/librebar_ml_core.so`
  - `build-android/lib/librebar_ml_jni_core.so`
  - `build-android/lib/rebar_ml.jar`
  - (Includes the built-in `com.arm.rebarml.Tee` node; no extra artifacts.)
- Optional model nodes (build + package what you use):
  - WAV → PCM:
    - `build-android/lib/librebar_ml_model_wavtopcm.so`
    - `build-android/lib/librebar_ml_jni_model_wavtopcm.so`
    - `build-android/lib/rebar_ml_model_wavtopcm.jar`
  - PCM S16LE → PCM F32LE:
    - `build-android/lib/librebar_ml_model_pcms16letof32le.so`
    - `build-android/lib/librebar_ml_jni_model_pcms16letof32le.so`
    - `build-android/lib/rebar_ml_model_pcms16letof32le.jar`
  - Moonshine STT (stub backend in this repo):
    - `build-android/lib/librebar_ml_model_moonshine_stt.so`
    - `build-android/lib/librebar_ml_jni_model_moonshine_stt.so`
    - `build-android/lib/rebar_ml_model_moonshine_stt.jar`
  - CF Moonshine STT (Compute Flow-backed, requires `-DUSE_CF=ON`):
    - `build-android/lib/librebar_ml_model_cf_moonshine_stt.so`
    - `build-android/lib/librebar_ml_jni_model_cf_moonshine_stt.so`
    - `build-android/lib/rebar_ml_model_cf_moonshine_stt.jar`
  - MNN Omni (stub backend in this repo):
    - `build-android/lib/librebar_ml_model_mnn_omni.so`
    - `build-android/lib/librebar_ml_jni_model_mnn_omni.so`
    - `build-android/lib/rebar_ml_model_mnn_omni.jar`

## 2) Create an Android module for the artifacts (recommended)

Create an Android library module (for example `:rebar-prebuilt`) and place the
artifacts like this (example for `arm64-v8a`):

```text
rebar-prebuilt/
  libs/
    rebar_ml.jar
    rebar_ml_model_wavtopcm.jar
    rebar_ml_model_pcms16letof32le.jar
    # + other rebar_ml_model_*.jar you use
  src/main/jniLibs/arm64-v8a/
    librebar_ml_core.so
    librebar_ml_jni_core.so
    librebar_ml_model_wavtopcm.so
    librebar_ml_jni_model_wavtopcm.so
    librebar_ml_model_pcms16letof32le.so
    librebar_ml_jni_model_pcms16letof32le.so
    # + other librebar_ml_model_*.so / librebar_ml_jni_model_*.so you use
```

Gradle wiring (Groovy):

```gradle
android {
  defaultConfig {
    ndk {
      abiFilters "arm64-v8a"
    }
  }
}

dependencies {
  api files("libs/rebar_ml.jar")
  api files("libs/rebar_ml_model_wavtopcm.jar")
  api files("libs/rebar_ml_model_pcms16letof32le.jar")
  // api files("libs/rebar_ml_model_moonshine_stt.jar")
  // api files("libs/rebar_ml_model_cf_moonshine_stt.jar") // if built with USE_CF=ON
  // api files("libs/rebar_ml_model_mnn_omni.jar")
}
```

If you enable minification (R8/Proguard), keep rules similar to:

```text
-keep class com.arm.rebarml.** { *; }
-keep class com.arm.rebarml.models.** { *; }
```

## 2b) Android Studio + CMake (externalNativeBuild) (optional)

If your app already builds native code via Android Studio’s
`externalNativeBuild { cmake { ... } }`, the simplest approach is still to
consume **prebuilt** rebar-ml artifacts (this document) and a **prebuilt** Compute
Flow “Application SDK” directory, and then link them from your app’s CMake.

Key points:

- Do **not** add the Compute Flow **source tree** via `add_subdirectory(...)`.
  The Compute Flow project sets global CMake options (notably
  `CMAKE_CXX_STANDARD 17`) and is not intended to be embedded that way.
- Instead, build CF separately (typically via the Brick Manager / `compute-flow
  build ...`) and use the generated SDK directory (see
  the Compute Flow repo: `docs/howto/developing_apps_with_compute_flow.md`).
- Brick Manager is a **build-time** tool: use it to install bricks/models and
  produce the SDK. You do not run Brick Manager on the Android device.

### Build the Compute Flow SDK (Android, arm64-v8a)

From the repo root (the directory containing `CMakePresets.json`):

```sh
export ANDROID_NDK=/path/to/android-ndk
export NDK_PATH="$ANDROID_NDK"                      # used by rebar-ml presets

CF_SRC_DIR="$PWD/cf/compute-flow"                   # compute-flow checkout location (cloned if missing)
CF_HOME="$HOME/.cache/rebar/compute-flow/cf-home"   # Brick Manager state (cache, tools, models)

# Recommended: one-shot provisioning script (installs stt:moonshine + moonshine-base-en by default)
python3 scripts/dev/provision_compute_flow.py \
  --target android_arm64 \
  --android-ndk "$ANDROID_NDK" \
  --cf-src-dir "$CF_SRC_DIR" \
  --cf-home "$CF_HOME" \
  --skip-if-ready
```

To pin the Compute Flow revision in a scripted setup, add `--cf-ref <tag/sha>`
(and optionally `--cf-repo-url ...`).

Manual equivalent (Brick Manager commands):

```sh
export COMPUTE_FLOW_ANDROID_NDK_ROOT="$ANDROID_NDK"
export COMPUTE_FLOW_HOME="$CF_HOME"

cd "$CF_SRC_DIR"
uv sync --frozen
uv run compute-flow update
uv run compute-flow brick install stt:moonshine
uv run compute-flow model install stt:moonshine moonshine-base-en
uv run compute-flow build android_arm64
```

The SDK directory will be at `.../build_android_arm64/sdk/` under the Compute Flow source
directory.

Notes:

- `compute-flow brick install stt:moonshine` installs the brick **source** (it does not download
  model packages). `compute-flow brick install --all` installs every cached brick variant.
- `compute-flow model install ...` downloads model packages (for Moonshine this is from Hugging
  Face). If you hit auth/rate-limit issues, set `HF_TOKEN` or configure `~/.netrc`.
- `compute-flow build android_arm64` currently configures `ANDROID_PLATFORM`
  as `android-28` (minSdk 28). Keep your app’s `minSdkVersion` compatible.

### Option: auto-provision CF during rebar-ml CMake configure (simplest maintainer workflow)

If you enable Compute Flow integration in the rebar-ml build and turn on
auto-provisioning, rebar-ml will run the Brick Manager steps for you during the
CMake configure step (install bricks/models, build, and produce the SDK
directory).

This uses a Compute Flow checkout. By default, rebar-ml clones a managed checkout under
`COMPUTE_FLOW_ARTIFACTS_DIR/cf-src/` using `COMPUTE_FLOW_REPO_URL` +
`COMPUTE_FLOW_REF`. If you already have a pinned checkout (for example via a git
submodule), point rebar-ml at it with `COMPUTE_FLOW_SRC_DIR`.

This trades longer/less-predictable configure time for a simpler workflow
(no separate Gradle “CF provision” task required).

Example:

```sh
export ANDROID_NDK=/path/to/android-ndk
export NDK_PATH="$ANDROID_NDK"                      # used by rebar-ml presets

cmake --preset=x-android-aarch64 -B build-android \
  -DBUILD_REBAR=ON \
  -DUSE_CF=ON \
  -DCOMPUTE_FLOW_ARTIFACTS_DIR="$HOME/.cache/rebar/compute-flow" \
  -DCOMPUTE_FLOW_REPO_URL="https://github.com/ARM-software/compute-flow.git" \
  -DCOMPUTE_FLOW_REF="main" \
  -DCOMPUTE_FLOW_TARGET=android_arm64 \
  -DCOMPUTE_FLOW_ANDROID_NDK="$ANDROID_NDK"

cmake --build ./build-android
```

Minimal example `app/src/main/cpp/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_app_jni LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

add_library(app_jni SHARED app_jni.cpp)

# rebar-ml prebuilt .so's (packaged under src/main/jniLibs/<abi>/).
add_library(rebar_ml_core SHARED IMPORTED)
set_target_properties(rebar_ml_core PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../jniLibs/${ANDROID_ABI}/librebar_ml_core.so")

add_library(rebar_ml_jni_core SHARED IMPORTED)
set_target_properties(rebar_ml_jni_core PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../jniLibs/${ANDROID_ABI}/librebar_ml_jni_core.so")

target_link_libraries(app_jni PRIVATE rebar_ml_jni_core)
```

Notes:

- If you built rebar-ml with `-DUSE_CF=ON`, `librebar_ml_core.so` already links the Compute Flow SDK, so
  your app should not link `compute_flow::sdk` again.

### Automate CF + rebar-ml builds with Gradle (example)

If you want Android Studio builds to “just work” after cloning a repo that contains rebar-ml + CF, use
Gradle `Exec` tasks to provision the host-side dependencies:

- (Optionally) build the Compute Flow Android SDK (`.../compute-flow/build_android_arm64/sdk/`).
- Build rebar-ml Android artifacts (`build-android/lib/*.so` + `*.jar`), optionally auto-provisioning CF.
- Stage rebar-ml outputs under `app/build/generated/...` so they are packaged and on the classpath.

Assumptions:

- Your Android project vendors this repo at `third_party/rebar/large-language-models` (adjust
  `REBAR_ROOT`).
- You are targeting `arm64-v8a` only.
- Your host has `bash`, `cmake`, `python3` and `uv` on `PATH`.
- Your app uses `externalNativeBuild` only if you have your own JNI library. If you do not, you can
  omit the `externalNativeBuild` block below.

Example `app/build.gradle.kts` (Kotlin DSL):

```kotlin
import org.gradle.api.tasks.Copy
import org.gradle.api.tasks.Exec

val rebarRoot = file("$rootDir/third_party/rebar/large-language-models") // <-- adjust
val cfArtifactsDir = file("${gradle.gradleUserHomeDir}/caches/rebar/compute-flow")
val cfRepoUrl = "https://github.com/ARM-software/compute-flow.git"
val cfRef = "main"

val rebarBuildDir = file("$rebarRoot/build-android")
val rebarOutDir = file("$rebarBuildDir/lib")

val ndkRoot = providers.environmentVariable("ANDROID_NDK")
  .orElse(providers.environmentVariable("ANDROID_NDK_HOME"))
  .orElse(providers.environmentVariable("ANDROID_NDK_ROOT"))
  .orNull ?: error("Set ANDROID_NDK (or ANDROID_NDK_HOME) to your installed NDK root")

val jobs = Runtime.getRuntime().availableProcessors().coerceAtLeast(1)

tasks.register<Exec>("rebarBuildAndroidArm64") {
  workingDir = rebarRoot
  environment("NDK_PATH", ndkRoot) // consumed by the x-android-aarch64 preset
  commandLine("bash", "-lc", """
    set -euo pipefail
    cmake --preset=x-android-aarch64 -B "${rebarBuildDir.absolutePath}" \
      -DBUILD_REBAR=ON \
      -DUSE_CF=ON \
      -DCOMPUTE_FLOW_ARTIFACTS_DIR="${cfArtifactsDir.absolutePath}" \
      -DCOMPUTE_FLOW_REPO_URL="$cfRepoUrl" \
      -DCOMPUTE_FLOW_REF="$cfRef" \
      -DCOMPUTE_FLOW_TARGET=android_arm64 \
      -DCOMPUTE_FLOW_ANDROID_NDK="$ndkRoot"
    cmake --build "${rebarBuildDir.absolutePath}"
  """.trimIndent())
  outputs.dir(rebarOutDir)
}

val stagedJniLibs = file("$buildDir/generated/jniLibs/arm64-v8a")
val stagedJars = file("$buildDir/generated/jars")

tasks.register<Copy>("stageRebarJniLibs") {
  dependsOn("rebarBuildAndroidArm64")
  from(rebarOutDir) { include("librebar*.so") }
  into(stagedJniLibs)
}

tasks.register<Copy>("stageRebarJars") {
  dependsOn("rebarBuildAndroidArm64")
  from(rebarOutDir) { include("rebar*.jar") }
  into(stagedJars)
}

tasks.named("preBuild").configure { dependsOn("stageRebarJniLibs", "stageRebarJars") }

android {
  defaultConfig {
    ndk { abiFilters += "arm64-v8a" }
  }

  // Package the generated .so files.
  sourceSets["main"].jniLibs.srcDir(file("$buildDir/generated/jniLibs"))
}

dependencies {
  implementation(files("$stagedJars/rebar_ml.jar"))
  // implementation(files("$stagedJars/rebar_ml_model_wavtopcm.jar"))
  // + any other rebar_ml_model_*.jar you built/staged.
}
```

Notes:

- `compute-flow update` may touch the network. In CI you might prefer to run it explicitly, commit
  a pinned registry, or disable it in normal incremental builds.
- Your Android app does not need to add Compute Flow to its own JNI build if it only uses rebar-ml.
  When rebar-ml is built with `-DUSE_CF=ON`, Compute Flow is linked into `librebar_ml_core.so`.

## 3) Load libraries and use rebar-ml from Kotlin/Java

rebar-ml’s Java surface loads its JNI libraries automatically via static
initializers:

- Core: `com.arm.rebarml.RebarML` loads `rebar_ml_jni_core`
- Each model wrapper loads its own `rebar_ml_jni_model_*` library (for example
  `com.arm.rebarml.models.utils.wavtopcm.NativeBindings` loads
  `rebar_ml_jni_model_wavtopcm`)
- Built-in nodes (for example `com.arm.rebarml.Tee`) are provided by `rebar_ml.jar`
  + `rebar_ml_jni_core` and do not require extra artifacts.

As long as the `.so` files are packaged under `jniLibs/<abi>/`, you typically
do not need to call `System.loadLibrary(...)` yourself.

Minimal example (WAV → PCM, which works in this repo):

```kotlin
import com.arm.rebarml.*
import com.arm.rebarml.models.utils.wavtopcm.WavToPcm
import java.nio.ByteBuffer

// rebar-ml classes implement AutoCloseable. If your Kotlin stdlib does not provide
// AutoCloseable.use, define this helper (or use try/finally).
inline fun <T : AutoCloseable, R> T.use(block: (T) -> R): R =
    try { block(this) } finally { close() }

fun decodeWavToPcm(wavBytes: ByteArray): ByteArray {
    WavToPcm.createNode(16_000, 1).use { wavToPcm ->
        wavToPcm.createPipelineSession().use { pipeline ->
            pipeline.getNodeSession(wavToPcm).use { session ->
                session.getAudioInput().use { audioIn ->
                    session.getAudioOutput().use { audioOut ->
                        AudioData(
                            RebarBuffer.wrapInput(ByteBuffer.wrap(wavBytes)),
                            AudioFormat(AudioCodec.WAV, 0, 0),
                            "en"
                        ).use { wav ->
                            audioIn.send(wav)
                        }

                        pipeline.finishInputs()

                        val first = audioOut.receive()
                        check(first.status() == ReceiveStatus.SUCCESS && first.item() != null)
                        val pcm = first.item()!!.data()
                        pcm.use {
                            val buf = it.bytes().buffer()
                            val out = ByteArray(buf.remaining())
                            buf.get(out)
                            return out
                        }
                    }
                }
            }
        }
    }
}
```

Minimal example (WAV → transcript via CF Moonshine STT):

```kotlin
import com.arm.rebarml.*
import com.arm.rebarml.models.cf.stt.moonshine.MoonshineStt
import com.arm.rebarml.models.utils.pcms16letof32le.PcmS16leToF32le
import com.arm.rebarml.models.utils.wavtopcm.WavToPcm
import java.nio.ByteBuffer

fun transcribeWavWithCfMoonshine(wavBytes: ByteArray, manifestPath: String): String {
    WavToPcm.createNode(16_000, 1).use { wavToPcm ->
        PcmS16leToF32le.createNode(16_000, 1).use { pcmToF32 ->
            MoonshineStt.createNode(manifestPath, /*threads*/ 2, /*warmup*/ false).use { stt ->
                wavToPcm.getAudioOutputPort().use { wavOut ->
                    pcmToF32.getAudioInputPort().use { pcmToF32In ->
                        wavOut.connect(pcmToF32In)
                    }
                }
                pcmToF32.getAudioOutputPort().use { pcmToF32Out ->
                    stt.getAudioInputPort().use { sttIn ->
                        pcmToF32Out.connect(sttIn)
                    }
                }

                wavToPcm.createPipelineSession().use { pipeline ->
                    pipeline.getNodeSession(wavToPcm).use { wavSession ->
                        pipeline.getNodeSession(stt).use { sttSession ->
                            val audioIn = wavSession.getAudioInput()
                            val transcriptOut = sttSession.getTextOutput()
                            check(audioIn != null && transcriptOut != null)

                            AudioData(
                                RebarBuffer.wrapInput(ByteBuffer.wrap(wavBytes)),
                                AudioFormat(AudioCodec.WAV, 0, 0),
                                "en"
                            ).use { wav ->
                                audioIn.send(wav)
                            }

                            pipeline.finishInputs()

                            val first = transcriptOut.receive()
                            check(first.status() == ReceiveStatus.SUCCESS && first.item() != null)
                            return first.item()!!.data().text()
                        }
                    }
                }
            }
        }
    }
}
```

For staged submissions, call `audioIn.add(...)` and then `pipeline.submit()`.
See `ARCH.md` for the `add()`/`submit()`/`send()` semantics and constraints.

### Fan-out example (STT → Tee → LLM + UI transcript tap)

Because rebar-ml output ports are one-to-one, use the `Tee` node to duplicate a
stream to multiple consumers. A common pattern is:

- STT text output → tee input.
- Tee output 0 → LLM text input.
- Tee output 1 left **unconnected** so the Android app can read it via
  `RebarNodeSession.getTextOutput(1)` and update the UI.

```kotlin
import com.arm.rebarml.*
import com.arm.rebarml.models.cf.stt.moonshine.MoonshineStt
import com.arm.rebarml.models.llmframework.mnnomni.MnnOmni

fun wireSttToLlmWithUiTap(manifestPath: String) {
    MoonshineStt.createNode(manifestPath, /*threads*/ 4, /*warmup*/ true).use { stt ->
        Tee.createNode(/*outputs*/ 2).use { tee ->
            MnnOmni.createNode(/*sr*/ 16_000, /*ch*/ 1).use { llm ->
                stt.getTextOutputPort().use { sttOut ->
                    tee.getTextInputPort().use { teeIn ->
                        sttOut.connect(teeIn)
                    }
                }
                tee.getTextOutputPort(0).use { teeOut0 ->
                    llm.getTextInputPort().use { llmIn ->
                        teeOut0.connect(llmIn)
                    }
                }

                stt.createPipelineSession().use { pipeline ->
                    pipeline.getNodeSession(stt).use { sttSession ->
                        pipeline.getNodeSession(tee).use { teeSession ->
                            val audioIn = sttSession.getAudioInput()
                            val transcriptTap = teeSession.getTextOutput(1) // unconnected output
                            check(audioIn != null)
                            check(transcriptTap != null)

                            // Send audio into STT via audioIn.send(...)
                            // Read transcriptTap.receive() on a background thread and post to UI.
                        }
                    }
                }
            }
        }
    }
}
```

Notes:

- This repo’s `mnn_omni` backend is a stub; replace it for production use (see
  `MIGRATION_GUIDE.md`).
- The CF-backed Moonshine STT node expects PCM F32LE mono. For WAV or PCM S16LE inputs, insert
  `wav-to-pcm` and/or `pcms16letof32le` upstream.
- If you connect tee output 1 as well, `getTextOutput(1)` will return `null`
  because connected outputs are not exposed as output streams.

## 4) Model/assets paths on Android

Some nodes accept filesystem paths (for example `MoonshineStt.createNode(...)`
takes a `manifestPath`). Android app assets are not regular files, so you will
typically copy any required asset(s) to internal storage and pass that path to
native code.

Example helper (copy one asset into `context.filesDir` and return its path):

```kotlin
fun materializeAsset(context: android.content.Context, assetName: String): String {
    val outFile = java.io.File(context.filesDir, assetName)
    if (!outFile.exists()) {
        context.assets.open(assetName).use { input ->
            outFile.outputStream().use { output -> input.copyTo(output) }
        }
    }
    return outFile.absolutePath
}
```

### Compute Flow models (brick packages)

Compute Flow models are distributed as **model packages** (a directory containing one or more
manifests plus model/tokenizer/etc files referenced by those manifests). When you use a CF-backed
node/brick, you typically pass a **manifest JSON path** and the manifest resolves the rest of the
files via relative paths.

For example, the `stt:moonshine` `moonshine-base-en` package contains a manifest named
`moonshine_manifest_base_en.json`.

Download a model package on your development machine (writes under `$COMPUTE_FLOW_HOME/models/...`):

```sh
export COMPUTE_FLOW_HOME="$PWD/.compute-flow"
cd cf/compute-flow
uv sync
uv run compute-flow update

# Example: STT + VAD + TTS.
uv run compute-flow model install stt:moonshine moonshine-base-en
uv run compute-flow model install vad:silero-vad silero-fp32
uv run compute-flow model install tts:qwen3-tts qwen3-tts-0-6b-custom-voice

# Locate the installed package directory (print path only).
PACKAGE_DIR="$(uv run compute-flow model path tts:qwen3-tts qwen3-tts-0-6b-custom-voice)"
```

To ship a model package in your Android app, copy the **entire** package directory into APK assets
or download it at runtime, then materialize it into internal storage so you can pass a filesystem
path to the manifest (assets are not seekable files).

#### Automate model downloads with Gradle (example)

You can run Brick Manager model installation as a Gradle step (host-side). The task below:

- keeps Compute Flow state under the repo (`$rootDir/.compute-flow/`)
- downloads one or more model packages under `$COMPUTE_FLOW_HOME/models/...`

Example (Kotlin DSL) to place in your Android app module’s `build.gradle.kts`:

```kotlin
tasks.register<Exec>("cfInstallModels") {
    // Adjust if CF lives somewhere else in your repo layout.
    workingDir = file("$rootDir/cf/compute-flow")
    environment("COMPUTE_FLOW_HOME", "$rootDir/.compute-flow")

    // Private Hugging Face repos require HF_TOKEN / `hf auth login`.
    // environment("HF_TOKEN", providers.environmentVariable("HF_TOKEN").orNull)

    commandLine("bash", "-lc", """
        set -euo pipefail
        uv sync
        uv run compute-flow update
        uv run compute-flow model install stt:moonshine moonshine-base-en
        uv run compute-flow model install vad:silero-vad silero-fp32
    """.trimIndent())
}
```

#### Export models to a phone (two practical options)

Option A (small models): **package them in APK assets**

- Copy the installed model package directory into `app/src/main/assets/...` as part of the build.
- On first run, recursively copy that asset directory to internal storage and pass the manifest path
  to native code.

Option B (large models, dev-friendly): **push them with `adb` into app external files**

1. Install your debug build (`installDebug`) so the external app directory exists.
2. Push the model package(s) from your host:

```sh
APP_ID=com.example.app
export COMPUTE_FLOW_HOME="$PWD/.compute-flow"

PACKAGE_DIR="$(cd cf/compute-flow && uv run compute-flow model path stt:moonshine moonshine-base-en)"
DEST="/sdcard/Android/data/${APP_ID}/files/compute-flow/models/stt/moonshine/moonshine-base-en"
adb shell mkdir -p "${DEST}"
adb push "${PACKAGE_DIR}/." "${DEST}/"
```

In the app, use `context.getExternalFilesDir(null)` as the base directory and construct the
manifest path from there.

## 5) Troubleshooting

Common issues during integration:

- `java.lang.UnsatisfiedLinkError: no rebar_ml_jni_core in java.library.path`
  - The app did not package `librebar_ml_jni_core.so` for the running ABI.
  - Ensure your device/emulator ABI matches `jniLibs/<abi>/` (for example,
    `arm64-v8a` vs `x86_64`).

- `dlopen failed: library "librebar_ml_core.so" not found`
  - Package `librebar_ml_core.so` alongside `librebar_ml_jni_core.so` in the same
    `jniLibs/<abi>/` directory.
  - Also package any `librebar_ml_model_*.so` libraries required by the model JNI
    libraries you load.

- `dlopen failed: library "libc++_shared.so" not found`
  - Package `libc++_shared.so` in the same `jniLibs/<abi>/` folder (many apps
    already do this when shipping other NDK components).
