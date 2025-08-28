#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
import json
import logging
import os
import shutil
import subprocess
import sys
import tarfile
import re
from argparse import ArgumentParser
from enum import Enum
from pathlib import Path

# Logging configuration
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class Architecture(Enum):
    X86 = "X86"
    ANDROID_ARM64 = "ANDROID_ARM64"
    AARCH64 = "AARCH64"

# === Configuration ===
BAZEL_COMMAND   = "bazel"
GIT_COMMAND     = "git"
TARGET_DIR      = "mediapipe/tasks/cc/genai/inference/c"
LIB_NAME        = "libllm_inference_engine_cpu.so"

def get_gcc_version(base_path):
    """
    Get the ARM GNU toolchain's gcc version

    :param base_path: ARM GNU toolchain's root directory path
    :return: A list containing gcc-version as bazel flag
    """
    cmd = [base_path +'/bin/aarch64-none-linux-gnu-gcc', '--version']
    try:
        output = subprocess.run(cmd,capture_output=True,text=True)
        match = re.search(r'\b\d+\.\d+\.\d+\b', output.stdout)
        if match:
            logger.info("ARM GNU GCC version:"+(match.group(0)))
        else:
            logger.error("ARM GNU GCC version not found")
        return [f'--define=gcc_version={(match.group(0))}']
    except subprocess.CalledProcessError as e:
        logger.error("Error finding gcc version ,verify toolchain root directory: %s", e)

def setup_default_toolchain(mediapipe_dir, toolchain_tar_path):
    """
    Copy and extract default toolchain to mediapipe dir

    :param mediapipe_dir: The root directory of the mediapipe project.
    :param toolchain_tar_path: Path to ARM GNU toolchain tar file
    :return: Extracted path to ARM GNU toolchain's root path
    """
    # Extract the Arm GNU Toolchain
    with tarfile.open(toolchain_tar_path, "r:xz") as tar:
        tar.extractall(path=mediapipe_dir)

    # Get the top-level directory name from the toolchain tarball
    toolchain_root_dir = os.path.abspath(toolchain_tar_path).split(".tar")[0].split("/")[-1]

    toolchain_root = os.path.join(os.path.abspath(mediapipe_dir), toolchain_root_dir)

    return toolchain_root

def setup_aarch64_build(mediapipe_dir, toolchain_config):
    """
    Sets up the Arm GNU Toolchain Path and Bazel configuration.

    @param mediapipe_dir The root directory of the mediapipe project.
    @param toolchain_config Path to the Bazel toolchain configuration tar archive.
    """

    os.makedirs(mediapipe_dir, exist_ok=True)

    logger.info(f"Extracting toolchain config to: {mediapipe_dir}")

    dst_folder= os.path.join(mediapipe_dir,toolchain_config.split("/")[-1])

    os.makedirs(dst_folder, exist_ok=True)

    # Copy the entire folder, including all its contents
    shutil.copytree(toolchain_config, dst_folder, dirs_exist_ok=True)

def copy_lib_to_build_dir(filename, search_dir, dest_dir):
    """
    Search for the library file in a search directory and copy to provided destination directory.

    @param filename:    Name of the file to search for.
    @param search_dir:  Root directory to begin search.
    @param dest_dir:    Destination directory to copy the file to.
    """
    source_path = os.path.join(search_dir, TARGET_DIR, filename)
    dest_path   = os.path.join(dest_dir, filename)

    if not os.path.exists(source_path):
        logger.error(f"Source file does not exist: {source_path}")
        return
    if not os.path.exists(dest_path):
        dest_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy(source_path, dest_path)
        logger.info(f"Copied {filename} to {dest_dir}")
    else:
        logger.info(f"File '{filename}' already exists, not copying")

def copy_lib_to_jni_dir(filename, search_dir, dest_dir):
    """
    Search for the library file in a search directory and copy to provided destination directory.

    @param filename:    Name of the file to search for.
    @param search_dir:  Root directory to begin search.
    @param dest_dir:    Destination directory to copy the file to.
    """
    source_path = os.path.join(search_dir, TARGET_DIR, filename)
    dest_path   = os.path.join(dest_dir, filename)

    if not os.path.exists(source_path):
        logger.error(f"Source file does not exist: {source_path}")
        return
    if not os.path.exists(dest_path):
        dest_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy(source_path, dest_path)
        logger.info(f"Copied {filename} to {dest_dir}")
    else:
        logger.info(f"File '{filename}' already exists, not copying")

def check_and_set_android_ndk(mediapipe_dir, android_ndk_dir):
    """
    Check if the android_ndk_repository is already defined in Bazel's workspace.
    If not, append the required configuration to the WORKSPACE file.

    @param mediapipe_dir:       directory to use bazel query
    @param android_ndk_dir:     android NDK directory.
    """
    print("Setting android NDK")
    android_ndk_config = """
android_ndk_repository(
    name = "androidndk",
    api_level = 21,
    path = "{ndk_path}",
)

# See https://github.com/bazelbuild/rules_android_ndk/issues/31#issuecomment-1396182185
bind(
    name = "android/crosstool",
    actual = "@androidndk//:toolchain"
)
""".format(ndk_path=android_ndk_dir)

    cmd = [BAZEL_COMMAND, "query"] + ['"kind(android_ndk_repository, //external:all)"']
    logger.info("Running command: %s", " ".join(cmd))
    try:
        subprocess.run(cmd, cwd=mediapipe_dir, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        logger.info("android_ndk_repository already exists. No action needed.")
    except subprocess.CalledProcessError as e:
        logger.warning("android_ndk_repository not found. Adding configuration to WORKSPACE.")
        workspace_file = mediapipe_dir / "WORKSPACE"
        try:
            with open(workspace_file, "a") as f:
                f.write(android_ndk_config)
            logger.info("Android NDK configuration appended to WORKSPACE.")
        except IOError as io_err:
            logger.error("Failed to write to WORKSPACE file: %s", io_err)

def load_config(config_path):
    """
    Load the basic bazel build configuration

    @param config_path:     path for configuration file
    """
    try:
        with open(config_path, "r") as f:
            return json.load(f)
    except (IOError, json.JSONDecodeError) as e:
        logger.error("Failed to load configuration file: %s", e)
        sys.exit(1)

def get_bazel_flags(config: dict, target_arch: str) -> list[str]:
    """
    Returns a list of Bazel build flags based on the specified target architecture.
    @param dict: Dictionary containing bazel falgs for each Architecture
    @param target_arch: The target architecture defined by Architecture enum
    """
    target_arch.strip().upper()
    common = config.get("common_flags", [])
    # Compute optimum number of parallel jobs (cores - 2), but at least 1
    parallel_jobs = max(os.cpu_count() - 2, 1)
    common.append(f"--jobs={parallel_jobs}")
    target_specific = config.get("target_flags", {}).get(target_arch, [])
    return common + target_specific

def check_bazel_installed():
    """
    Check that bazel is installed by invoking bazel command and checking version.
    """
    try:
        subprocess.run([BAZEL_COMMAND, "--version"], check=True, stdout=subprocess.PIPE)
    except FileNotFoundError:
        logger.error("Bazel is not installed or not in PATH.")
        sys.exit(1)
def get_python_version_flags():
    """
    Add a flag for python version for bazel build to use.

    @return A list containing bazel flag for hermitic-python version
    """
    python_version = '.'.join(map(str, sys.version_info[:2]))
    logger.info("Adding hermetic python bazel flag for python version: %s", python_version)
    return ["--repo_env=HERMETIC_PYTHON_VERSION=" + python_version]
def apply_patch(patch_file, mediapipe_dir):
    """
    Applies a patch file to the current Git repo.

    @param patch_file:      patch file to apply.
    @param mediapipe_dir:   directory to run the apply command in.
    """
    if not patch_file.exists():
        logger.error("Patch file not found: %s", patch_file)
        return
    try:
        cmd = [GIT_COMMAND, "apply", patch_file]
        subprocess.run(cmd, cwd=mediapipe_dir, check=True)
        logger.info("Patch applied: %s", patch_file)
    except subprocess.CalledProcessError as e:
        logger.error("Error applying patch: %s", e)

def build_target(mediapipe_dir: Path, bazel_build_flags: list[str]):
    """
    Run bazel build command inside the mediapipe directory with provided build flags.

    @param mediapipe_dir:       directory to run the bazel build command in.
    @param bazel_build_flags:   build flags to supply to bazel build command.
    """
    cmd = [BAZEL_COMMAND, "build"] + bazel_build_flags + ["//" + TARGET_DIR + ":" + LIB_NAME]
    logger.info("Building MediaPipe target: %s", " ".join(cmd))
    try:
        subprocess.run(cmd, cwd=mediapipe_dir, check=True)
        print("Build completed successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Bazel build failed with error code {e.returncode}")
        sys.exit(e.returncode)

def main():
    parser = ArgumentParser()
    parser.add_argument(
        "--mediapipe-dir",
        help="Path to where mediapipe was downloaded.",
        required=True)

    parser.add_argument(
        "--build-dir",
        help="Path to where the built libraries should be copied.",
        required=True)

    parser.add_argument(
        "--target-arch",
        help="Target architecture to build mediapipe for.",
        required=True)

    parser.add_argument(
        "--android-ndk-dir",
        help="Path to Android NDK.")

    parser.add_argument(
        "--base-path",
        help="Path to toolchain.")

    parser.add_argument(
        "--android-copy-path",
        help="Path to copy mediapipe library"
    )

    parser.add_argument(
        "--use-dotprod",
        action="store_true",
        help="Enable use of dotprod optimizations"
    )

    parser.add_argument(
        "--use-i8mm",
        action="store_true",
        help="Enable use of i8mm optimizations"
    )

    parser.add_argument(
        "--use-kleidiai",
        action="store_true",
        help="Use kleidiai kernels to accelerate inference"
    )

    parser.add_argument(
        "--cross-compilation",
        help="To enable cross-compilation for Aarch64 target",
        action="store_true"
    )

    current_file_dir        = Path(__file__).parent.resolve()
    mediapipe_config_file   = current_file_dir / 'mediapipe-config.json'
    mediapipe_patch_file    = current_file_dir / 'mediapipe-update.patch'

    args = parser.parse_args()

    mediapipe_dir = Path(args.mediapipe_dir)
    build_dir = Path(args.build_dir)
    patch_file_path = Path(mediapipe_patch_file)
    target_arch = args.target_arch

    if args.android_copy_path:
       android_library_dest_dir = Path(args.android_copy_path)
    apply_patch(patch_file_path, mediapipe_dir)
    check_bazel_installed()
    if not os.path.exists(mediapipe_dir):
        logger.error("MediaPipe directory not found at %s", mediapipe_dir)
        sys.exit(1)

    if args.android_ndk_dir:
        check_and_set_android_ndk(mediapipe_dir, args.android_ndk_dir)

    config = load_config(mediapipe_config_file)
    bazel_build_flags = get_bazel_flags(config, target_arch)
    bazel_build_flags +=get_python_version_flags()

    if target_arch.strip().upper() == "AARCH64" :
        if args.cross_compilation or args.base_path:
            logger.info("Mediapipe is being cross-compiled")
            bazel_build_flags += config["cross_compilation"]
            toolchain_dir = os.path.join(os.path.abspath(current_file_dir),"../..",config["toolchain_tar"][0])
            toolchain_config_dir = os.path.join(os.path.abspath(current_file_dir),"toolchain")
            setup_aarch64_build(mediapipe_dir,toolchain_config_dir)
            if args.base_path :
                bazel_build_flags.append(f"--define=base_path={args.base_path}")
                bazel_build_flags += get_gcc_version(args.base_path)
            else :
                toolchain_root_path = setup_default_toolchain(mediapipe_dir,toolchain_dir)
                bazel_build_flags.append(f"--define=base_path={toolchain_root_path}")
                bazel_build_flags += get_gcc_version(toolchain_root_path)


    if args.use_i8mm:
        bazel_build_flags += config["i8mm"]
    if args.use_dotprod:
        bazel_build_flags += config["dotprod"]
    if args.use_kleidiai:
        bazel_build_flags += config["kleidiai"]
    build_target(mediapipe_dir, bazel_build_flags)
    mediapipe_build_dir = os.path.join(mediapipe_dir, "bazel-bin")
    copy_lib_to_build_dir(LIB_NAME, mediapipe_build_dir, build_dir)

    if args.android_copy_path:
        copy_lib_to_jni_dir(LIB_NAME,mediapipe_build_dir,android_library_dest_dir)

if __name__ == "__main__":
    main()