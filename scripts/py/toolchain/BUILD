#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
load(":cc_toolchain_config.bzl", "cc_toolchain_config")
package(default_visibility = ["//visibility:public"])
cc_toolchain_config(name = "aarch64_toolchain_config")

cc_toolchain_suite(
    name = "aarch64_suite",
    toolchains = {
        "aarch64": "aarch64_toolchain",
    },
)

filegroup(name = "empty")

cc_toolchain(
    name = "aarch64_toolchain",
    toolchain_identifier = "aarch64-toolchain",
    toolchain_config = ":aarch64_toolchain_config",
    all_files = ":empty",
    compiler_files = ":empty",
    dwp_files = ":empty",
    linker_files = ":empty",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 0,
)