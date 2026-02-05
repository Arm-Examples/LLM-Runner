#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Pin:
    framework: str
    var: str
    value: str
    file: Path
    line: int


_SET_RE = re.compile(
    r'^\s*set\s*\(\s*(?P<var>[A-Za-z0-9_]+)\s+"(?P<value>[^"]+)"',
    flags=re.IGNORECASE,
)


def _extract_pins(path: Path, framework: str, vars_of_interest: set[str]) -> list[Pin]:
    pins: list[Pin] = []
    for idx, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
        m = _SET_RE.match(line)
        if not m:
            continue
        var = m.group("var")
        if var not in vars_of_interest:
            continue
        pins.append(Pin(framework=framework, var=var, value=m.group("value"), file=path, line=idx))
    return pins


def _read_cache(cache_path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith(("//", "#")):
            continue
        m = re.match(r"^([^:=]+):[^=]*=(.*)$", line)
        if not m:
            continue
        out[m.group(1).strip()] = m.group(2).strip()
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description="Report pinned framework versions/commits used by this repo.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd(), help="Repo root (default: cwd).")
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="Build dir to inspect for overrides.")
    parser.add_argument("--json", action="store_true", help="Emit JSON instead of human output.")
    args = parser.parse_args()

    repo_root: Path = args.repo_root.resolve()
    build_dir: Path = (repo_root / args.build_dir).resolve()

    targets: list[tuple[str, Path, set[str]]] = [
        (
            "llama.cpp",
            repo_root / "src" / "cpp" / "frameworks" / "llama_cpp" / "CMakeLists.txt",
            {"LLAMA_GIT_URL", "LLAMA_GIT_SHA"},
        ),
        (
            "onnxruntime-genai",
            repo_root / "src" / "cpp" / "frameworks" / "onnxruntime_genai" / "CMakeLists.txt",
            {"ONNXRUNTIME_GIT_URL", "ONNXRUNTIME_GIT_TAG", "ONNXRT_GENAI_GIT_URL", "ONNXRT_GENAI_GIT_TAG"},
        ),
        (
            "mnn",
            repo_root / "src" / "cpp" / "frameworks" / "mnn" / "CMakeLists.txt",
            {"MNN_GIT_URL", "MNN_GIT_TAG"},
        ),
        (
            "mediapipe",
            repo_root / "src" / "cpp" / "frameworks" / "mediapipe" / "CMakeLists.txt",
            {"MEDIAPIPE_GIT_URL", "MEDIAPIPE_GIT_SHA"},
        ),
    ]

    pins: list[Pin] = []
    missing_files: list[str] = []
    for framework, path, vars_of_interest in targets:
        if not path.exists():
            missing_files.append(str(path))
            continue
        pins.extend(_extract_pins(path, framework, vars_of_interest))

    cache_path = build_dir / "CMakeCache.txt"
    cache: dict[str, str] = _read_cache(cache_path) if cache_path.exists() else {}

    if args.json:
        payload = {
            "repo_root": str(repo_root),
            "build_dir": str(build_dir),
            "pins": [
                {"framework": p.framework, "var": p.var, "value": p.value, "file": str(p.file), "line": p.line}
                for p in pins
            ],
            "cache_overrides": {k: v for k, v in cache.items() if k.endswith(("_GIT_TAG", "_GIT_SHA"))},
            "missing_files": missing_files,
        }
        print(json.dumps(payload, indent=2))
        return 0

    if missing_files:
        print("Missing expected files:")
        for f in missing_files:
            print(f"  - {f}")

    print("Pinned framework versions/commits (defaults in repo):")
    for p in sorted(pins, key=lambda x: (x.framework, x.var)):
        rel = p.file.relative_to(repo_root) if p.file.is_relative_to(repo_root) else p.file
        print(f"- {p.framework}: {p.var} = {p.value} ({rel}:{p.line})")

    if cache:
        print("\nBuild cache overrides (if any):")
        overrides = {
            k: v
            for k, v in cache.items()
            if k.endswith(("_GIT_TAG", "_GIT_SHA")) and v and k != "GATOR_GIT_TAG"
        }
        if not overrides:
            print("- (none)")
        else:
            for k, v in sorted(overrides.items()):
                print(f"- {k} = {v}")
    else:
        print("\nBuild cache overrides: (no build dir or CMakeCache.txt)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
