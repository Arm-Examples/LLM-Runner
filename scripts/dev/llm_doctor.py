#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


def _run(cmd: list[str]) -> tuple[int, str]:
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, check=False)
    except FileNotFoundError:
        return (127, "")
    out = (p.stdout or "") + (("\n" + p.stderr) if p.stderr else "")
    return (p.returncode, out.strip())


def _parse_version(s: str) -> tuple[int, int, int]:
    m = re.search(r"(\d+)\.(\d+)\.(\d+)", s)
    if not m:
        return (0, 0, 0)
    return (int(m.group(1)), int(m.group(2)), int(m.group(3)))


def _read_cmake_cache(cache_path: Path) -> dict[str, str]:
    cache: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith(("//", "#")):
            continue
        # Example: BUILD_JNI_LIB:BOOL=ON
        m = re.match(r"^([^:=]+):[^=]*=(.*)$", line)
        if not m:
            continue
        cache[m.group(1).strip()] = m.group(2).strip()
    return cache


def _extract_test_config_filenames(test_cmakelists: Path, llm_framework: str | None) -> list[str]:
    text = test_cmakelists.read_text(encoding="utf-8", errors="replace")

    if llm_framework:
        # Match the branch for the selected framework; capture the set(CONFIG_FILE_NAME ...) block.
        # This is a pragmatic regex; it assumes the current layout in test/CMakeLists.txt.
        pattern = (
            r'elseif\s*\(\s*\$\{LLM_FRAMEWORK\}\s+STREQUAL\s+"'
            + re.escape(llm_framework)
            + r'"\s*\)\s*'
            + r"(?:.|\n)*?"
            + r"set\s*\(\s*CONFIG_FILE_NAME(?P<body>(?:.|\n)*?)CACHE\s+STRING"
        )
        if llm_framework == "llama.cpp":
            pattern = (
                r'if\s*\(\s*\$\{LLM_FRAMEWORK\}\s+STREQUAL\s+"llama\.cpp"\s*\)\s*'
                + r"(?:.|\n)*?"
                + r"set\s*\(\s*CONFIG_FILE_NAME(?P<body>(?:.|\n)*?)CACHE\s+STRING"
            )
        m = re.search(pattern, text, flags=re.MULTILINE)
        if not m:
            return []
        body = m.group("body")
    else:
        # Extract all config filenames from any CONFIG_FILE_NAME set(...) block.
        bodies = re.findall(
            r"set\s*\(\s*CONFIG_FILE_NAME(?P<body>(?:.|\n)*?)CACHE\s+STRING", text, flags=re.MULTILINE
        )
        body = "\n".join(bodies)

    return re.findall(r"\"([^\"]+\.json)\"", body)


@dataclass(frozen=True)
class CheckResult:
    ok: bool
    label: str
    detail: str = ""


def _print_results(title: str, results: Iterable[CheckResult]) -> bool:
    print(f"\n== {title} ==")
    all_ok = True
    for r in results:
        status = "OK" if r.ok else "FAIL"
        all_ok = all_ok and r.ok
        if r.detail:
            print(f"{status}: {r.label} - {r.detail}")
        else:
            print(f"{status}: {r.label}")
    return all_ok


def main() -> int:
    parser = argparse.ArgumentParser(description="Environment and repo sanity checks for this LLM project.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd(), help="Path to repo root (default: cwd).")
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="CMake build dir to inspect.")
    parser.add_argument(
        "--llm-framework",
        type=str,
        default=None,
        choices=["llama.cpp", "onnxruntime-genai", "mediapipe", "mnn"],
        help="Override framework when inspecting test config wiring.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit a machine-readable JSON summary (still prints human output to stderr).",
    )
    args = parser.parse_args()

    repo_root: Path = args.repo_root.resolve()
    build_dir: Path = (repo_root / args.build_dir).resolve()

    # Collect report in parallel to printing.
    report: dict[str, object] = {"repo_root": str(repo_root), "build_dir": str(build_dir)}

    sys_info = [
        CheckResult(True, "OS", f"{platform.system()} {platform.release()}"),
        CheckResult(True, "Arch", platform.machine()),
        CheckResult(True, "Python", sys.version.split()[0]),
    ]

    cmake_rc, cmake_out = _run(["cmake", "--version"])
    cmake_ver = _parse_version(cmake_out)
    sys_info.append(
        CheckResult(cmake_rc == 0 and cmake_ver >= (3, 27, 0), "CMake", cmake_out.splitlines()[0] if cmake_out else "")
    )

    git_rc, git_head = _run(["git", "rev-parse", "HEAD"])
    sys_info.append(CheckResult(git_rc == 0, "Git HEAD", git_head if git_head else "git not found / not a repo?"))

    report["system"] = {
        "os": f"{platform.system()} {platform.release()}",
        "arch": platform.machine(),
        "python": sys.version.split()[0],
        "cmake": cmake_out.splitlines()[0] if cmake_out else None,
        "git_head": git_head if git_rc == 0 else None,
    }

    ok_sys = _print_results("System", sys_info)

    # Build/config inspection
    build_results: list[CheckResult] = []
    cache_path = build_dir / "CMakeCache.txt"
    cmake_error = build_dir / "CMakeFiles" / "CMakeError.log"
    cmake_output = build_dir / "CMakeFiles" / "CMakeOutput.log"

    cache: dict[str, str] = {}
    if cache_path.exists():
        cache = _read_cmake_cache(cache_path)
        llm_framework = args.llm_framework or cache.get("LLM_FRAMEWORK")
        build_results.append(CheckResult(True, "CMakeCache", str(cache_path)))
        if llm_framework:
            build_results.append(CheckResult(True, "LLM_FRAMEWORK", llm_framework))
        for k in ["BUILD_JNI_LIB", "BUILD_BENCHMARK", "BUILD_LLM_TESTING", "USE_KLEIDIAI", "CMAKE_BUILD_TYPE"]:
            if k in cache:
                build_results.append(CheckResult(True, k, cache[k]))
    else:
        llm_framework = args.llm_framework
        build_results.append(CheckResult(False, "CMakeCache", f"missing: {cache_path}"))

    # These logs are not guaranteed to exist for successful configurations; treat absence as informational.
    build_results.append(
        CheckResult(
            True,
            "CMakeError.log",
            str(cmake_error) if cmake_error.exists() else "missing (often normal on successful configure)",
        )
    )
    build_results.append(
        CheckResult(
            True,
            "CMakeOutput.log",
            str(cmake_output) if cmake_output.exists() else "missing (often normal on successful configure)",
        )
    )

    report["build"] = {
        "cache_present": cache_path.exists(),
        "llm_framework": llm_framework,
        "cache": {k: cache.get(k) for k in ["LLM_FRAMEWORK", "BUILD_JNI_LIB", "BUILD_BENCHMARK", "BUILD_LLM_TESTING", "USE_KLEIDIAI", "CMAKE_BUILD_TYPE"] if k in cache},
    }

    ok_build = _print_results("Build Dir", build_results)

    # Tools (JNI)
    tool_results: list[CheckResult] = []
    build_jni = cache.get("BUILD_JNI_LIB") if cache else None
    if build_jni in {"ON", "TRUE", "1"}:
        java_rc, java_out = _run(["java", "-version"])
        javac_rc, javac_out = _run(["javac", "-version"])
        tool_results.append(CheckResult(java_rc == 0, "java", java_out.splitlines()[0] if java_out else "missing"))
        tool_results.append(CheckResult(javac_rc == 0, "javac", javac_out.splitlines()[0] if javac_out else "missing"))
        tool_results.append(CheckResult(True, "JAVA_HOME", os.environ.get("JAVA_HOME", "<unset>")))
    else:
        tool_results.append(CheckResult(True, "JNI", "BUILD_JNI_LIB is OFF (or build dir missing)"))

    report["tools"] = {"java_home": os.environ.get("JAVA_HOME"), "build_jni_lib": build_jni}
    ok_tools = _print_results("Tooling", tool_results)

    # Test config wiring sanity
    wiring_results: list[CheckResult] = []
    test_cmakelists = repo_root / "test" / "CMakeLists.txt"
    model_cfg_dir = repo_root / "model_configuration_files"
    if test_cmakelists.exists():
        wiring_results.append(CheckResult(True, "test/CMakeLists.txt", str(test_cmakelists)))
        cfg_names = _extract_test_config_filenames(test_cmakelists, llm_framework)
        if llm_framework and not cfg_names:
            wiring_results.append(CheckResult(False, "CONFIG_FILE_NAME", f"could not locate config list for {llm_framework}"))
        else:
            wiring_results.append(CheckResult(True, "Config files referenced", str(len(cfg_names))))
        missing_cfg = []
        invalid_json = []
        for cfg in cfg_names:
            p = model_cfg_dir / cfg
            if not p.exists():
                missing_cfg.append(cfg)
                continue
            try:
                json.loads(p.read_text(encoding="utf-8"))
            except Exception:
                invalid_json.append(cfg)
        wiring_results.append(CheckResult(not missing_cfg, "Config files exist", ", ".join(missing_cfg) if missing_cfg else ""))
        wiring_results.append(CheckResult(not invalid_json, "Config JSON valid", ", ".join(invalid_json) if invalid_json else ""))
        report["test_wiring"] = {
            "llm_framework": llm_framework,
            "config_files": cfg_names,
            "missing": missing_cfg,
            "invalid_json": invalid_json,
        }
    else:
        wiring_results.append(CheckResult(False, "test/CMakeLists.txt", "missing"))

    ok_wiring = _print_results("Test Wiring", wiring_results)

    # Build artifacts (best-effort)
    artifacts: list[CheckResult] = []
    if build_dir.exists():
        llm_cpp_tests = build_dir / "bin" / "llm-cpp-tests"
        artifacts.append(CheckResult(llm_cpp_tests.exists(), "llm-cpp-tests", str(llm_cpp_tests) if llm_cpp_tests.exists() else "missing"))
        if build_jni in {"ON", "TRUE", "1"}:
            # Output directory is set to build/lib for shared libs in this repo.
            jni_so = build_dir / "lib" / "libarm-llm-jni.so"
            artifacts.append(CheckResult(jni_so.exists(), "arm-llm-jni", str(jni_so) if jni_so.exists() else "missing"))
    report["artifacts"] = {r.label: r.ok for r in artifacts}
    ok_artifacts = _print_results("Artifacts", artifacts)

    overall_ok = ok_sys and ok_build and ok_tools and ok_wiring and ok_artifacts

    if args.json:
        # Human output already printed; emit JSON to stdout for tooling.
        print(json.dumps({"ok": overall_ok, "report": report}, indent=2))
        return 0 if overall_ok else 1

    print("\n== Summary ==")
    print("OK" if overall_ok else "Issues detected (see FAIL items above).")
    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
