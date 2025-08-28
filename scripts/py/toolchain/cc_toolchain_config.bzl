#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load("@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl", "feature", "flag_group", "flag_set", "tool_path", "with_feature_set")

ALL_COMPILE_ACTIONS = [
    ACTION_NAMES.c_compile,
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.linkstamp_compile,
    ACTION_NAMES.assemble,
    ACTION_NAMES.preprocess_assemble,
    ACTION_NAMES.cpp_header_parsing,
    ACTION_NAMES.cpp_module_compile,
    ACTION_NAMES.cpp_module_codegen,
    ACTION_NAMES.clif_match,
    ACTION_NAMES.lto_backend,
]

ALL_LINK_ACTIONS = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

COMMON_OPT_COMPILE_FLAGS = [
    "-g0",
    "-O3",
    "-DNDEBUG",
    "-D_FORTIFY_SOURCE=2",
    "-ffunction-sections",
    "-fdata-sections",
]

NON_K8_OPT_COMPILE_FLAGS = [
    "-funsafe-math-optimizations",
    "-ftree-vectorize",
]

COMMON_LINKER_FLAGS = [
    "-lstdc++",
    "-lm",
    "-Wl,-no-as-needed",
    "-Wl,-z,relro,-z,now",
    "-Wall",
    "-pass-exit-codes",
]

def _impl(ctx):

    base_path = ctx.var.get("base_path", "")
    if not base_path:
        fail("Missing required toolchain argument: 'base_path'. Please pass --action_env=BASE_PATH=<toolchain root>.")
    
    # using 14.3.1 as default gcc_version
    gcc_version = ctx.var.get("gcc_version","14.3.1")

    gcc_path = base_path + "/bin"
    toolchain_path = base_path + "/aarch64-none-linux-gnu"
    gcc_versioned_include = base_path + "/lib/gcc/aarch64-none-linux-gnu/" + gcc_version

    ADDITIONAL_SYSTEM_INCLUDE_DIRECTORIES = [
        gcc_versioned_include + "/include",
        gcc_versioned_include + "/include-fixed",
        toolchain_path + "/include",
        toolchain_path + "/include/c++/" + gcc_version,
        toolchain_path + "/include/c++/" + gcc_version + "/backward",
        toolchain_path + "/include/c++/" + gcc_version + "/aarch64-none-linux-gnu",
        toolchain_path + "/libc/usr/include",
        toolchain_path,
    ]

    tool_paths = [
        tool_path(name = "gcc", path = gcc_path + "/aarch64-none-linux-gnu-gcc"),
        tool_path(name = "ld", path = gcc_path + "/aarch64-none-linux-gnu-ld"),
        tool_path(name = "ar", path = gcc_path + "/aarch64-none-linux-gnu-ar"),
        tool_path(name = "cpp", path = gcc_path + "/aarch64-none-linux-gnu-cpp"),
        tool_path(name = "gcov", path = gcc_path + "/aarch64-none-linux-gnu-gcov"),
        tool_path(name = "nm", path = gcc_path + "/aarch64-none-linux-gnu-nm"),
        tool_path(name = "objdump", path = gcc_path + "/aarch64-none-linux-gnu-objdump"),
        tool_path(name = "strip", path = gcc_path + "/aarch64-none-linux-gnu-strip"),
    ]

    def isystem_flags():
        result = []
        for d in ADDITIONAL_SYSTEM_INCLUDE_DIRECTORIES:
            result.append("-isystem")
            result.append(d)
        return result

    features = [

        feature(
            name = "cpp_compile_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [ACTION_NAMES.cpp_compile],
                    flag_groups = [flag_group(flags = ["-std=c++17", "-fPIC", "-fpermissive", "-ffunction-sections", "-fdata-sections"])]
                ),
            ]
        ),

        feature(
            name = "c_compile_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [ACTION_NAMES.c_compile],
                    flag_groups = [flag_group(flags = ["-fPIC", "-fno-strict-aliasing", "-Wno-incompatible-pointer-types"])]
                ),
            ]
        ),

        feature(
            name = "default_linker_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = ALL_LINK_ACTIONS,
                    flag_groups = [flag_group(flags = COMMON_LINKER_FLAGS)],
                ),
            ]
        ),

        feature(
            name = "unfiltered_compile_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = ALL_COMPILE_ACTIONS,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-no-canonical-prefixes",
                                "-fno-canonical-system-headers",
                                "-Wno-builtin-macro-redefined",
                                "-D__DATE__=\"redacted\"",
                                "-D__TIMESTAMP__=\"redacted\"",
                                "-D__TIME__=\"redacted\"",
                            ],
                        ),
                    ],
                ),
            ],
        ),

        feature(
            name = "default_compile_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = ALL_COMPILE_ACTIONS,
                    flag_groups = [flag_group(flags = ["-fPIC", "-U_FORTIFY_SOURCE", "-fstack-protector", "-Wall", "-Wunused-but-set-parameter", "-Wno-free-nonheap-object", "-fno-omit-frame-pointer"])],
                ),
                flag_set(
                    actions = ALL_COMPILE_ACTIONS,
                    flag_groups = [flag_group(flags = ["-march=armv8-a", "-g"])],
                    with_features = [with_feature_set(features = ["dbg"])],
                ),
                flag_set(
                    actions = ALL_COMPILE_ACTIONS,
                    flag_groups = [flag_group(flags = ["-march=armv8-a"] + COMMON_OPT_COMPILE_FLAGS + NON_K8_OPT_COMPILE_FLAGS)],
                    with_features = [with_feature_set(features = ["opt"])],
                ),
            ],
        ),

        feature(
            name = "custom_compile_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [
                        ACTION_NAMES.linkstamp_compile,
                        ACTION_NAMES.cpp_compile,
                        ACTION_NAMES.cpp_header_parsing,
                        ACTION_NAMES.cpp_module_compile,
                        ACTION_NAMES.cpp_module_codegen,
                        ACTION_NAMES.lto_backend,
                        ACTION_NAMES.clif_match,
                    ],
                    flag_groups = [
                        flag_group(
                            flags = ["-std=c++17"] + isystem_flags(),
                        ),
                    ],
                ),
                flag_set(
                    actions = [ACTION_NAMES.c_compile],
                    flag_groups = [
                        flag_group(
                            flags = ["-std=gnu11"] + isystem_flags(),
                        ),
                    ],
                ),
            ],
        ),
    ]

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        features = features,
        cxx_builtin_include_directories = ADDITIONAL_SYSTEM_INCLUDE_DIRECTORIES,
        toolchain_identifier = "aarch64-gcc-toolchain",
        host_system_name = "local",
        target_system_name = "local",
        target_cpu = "aarch64",
        target_libc = "unknown",
        compiler = "gcc",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        tool_paths = tool_paths,
    )

cc_toolchain_config = rule(
    implementation = _impl,
    attrs = {},
    provides = [CcToolchainConfigInfo],
)
