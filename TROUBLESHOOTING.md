<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->


# Troubleshooting

## Errors and fixes for Android Deployment

* Libomp library not found

```shell
    library "libomp.so" not found
```

Fix: Use -DGGML_OPENMP=OFF when building to avoid OpenMP dependency.

* Missing C++ bindings

```shell
    cannot locate symbol "_ZTVNSt6__ndk117bad_function_callE"
```

Fix: Ensure NDK is recent (r26b or newer). This error often relates to broken libc++ or stdc++ bindings.

* Missing shared libraries

```shell
    adb: error: cannot stat ...
```

Fix: Verify that the output binary or .so file exists in the expected path. Use ls build-android/bin/ or ls build-android/lib/ to confirm.

```shell
    CANNOT LINK EXECUTABLE: ...
```

Fix:

Ensure all required .so dependencies are pushed.
Set LD_LIBRARY_PATH=. to point to the local directory.
Use chmod +x on the binary after pushing.


## macOS Specific

* Aarch64 tools not on path

```shell
    zsh: command not found (for aarch64 tools)
```

Fix: Ensure NDK toolchain is in your path e.g:

```shell
export NDK_ROOT=/path/to/android-ndk-r26b export PATH=$NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64/bin:$PATH
```

Check with:

```shell
aarch64-linux-android-clang --version
```