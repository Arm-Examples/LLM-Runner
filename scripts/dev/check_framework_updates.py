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
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class FrameworkPin:
    name: str
    repo_url: str
    pin_type: str  # "tag" | "sha"
    pin_value: str


@dataclass(frozen=True)
class FrameworkUpdate:
    name: str
    pinned: str
    latest: str
    latest_type: str  # "tag" | "head"
    detail: str


_TAG_REF_RE = re.compile(r"refs/tags/(?P<tag>.+)$")
_SEMVER_TAG_RE = re.compile(r"^v?(?P<maj>\d+)\.(?P<min>\d+)\.(?P<pat>\d+)(?:\.(?P<rev>\d+))?$")


def _run(cmd: list[str]) -> tuple[int, str]:
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, check=False)
    except FileNotFoundError:
        return (127, "")
    out = (p.stdout or "") + (("\n" + p.stderr) if p.stderr else "")
    return (p.returncode, out.strip())


def _semver_key(tag: str) -> tuple[int, int, int, int] | None:
    m = _SEMVER_TAG_RE.match(tag.strip())
    if not m:
        return None
    return (
        int(m.group("maj")),
        int(m.group("min")),
        int(m.group("pat")),
        int(m.group("rev") or 0),
    )


def _latest_semver_tag(remote: str) -> str | None:
    rc, out = _run(["git", "ls-remote", "--tags", "--refs", remote])
    if rc != 0 or not out:
        return None
    tags: list[str] = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) != 2:
            continue
        ref = parts[1]
        m = _TAG_REF_RE.search(ref)
        if not m:
            continue
        tag = m.group("tag")
        if _semver_key(tag) is None:
            continue
        tags.append(tag)
    if not tags:
        return None
    return max(tags, key=lambda t: _semver_key(t) or (0, 0, 0, 0))


def _remote_head_sha(remote: str) -> str | None:
    rc, out = _run(["git", "ls-remote", remote, "HEAD"])
    if rc != 0 or not out:
        return None
    sha = out.split()[0].strip()
    return sha if re.fullmatch(r"[0-9a-f]{40}", sha) else None


def _read_framework_pins(repo_root: Path, build_dir: Path) -> list[FrameworkPin]:
    # Prefer the parsed view from our companion script (single source of truth).
    rc, out = _run(
        [
            sys.executable,
            str(repo_root / "scripts" / "dev" / "framework_versions.py"),
            "--repo-root",
            str(repo_root),
            "--build-dir",
            str(build_dir),
            "--json",
        ]
    )
    if rc != 0:
        raise RuntimeError("framework_versions.py failed")
    data = json.loads(out)

    def _get(var: str) -> str | None:
        for p in data.get("pins", []):
            if p.get("var") == var:
                return p.get("value")
        return None

    pins: list[FrameworkPin] = []

    llama_url = _get("LLAMA_GIT_URL")
    llama_sha = _get("LLAMA_GIT_SHA")
    if llama_url and llama_sha:
        pins.append(FrameworkPin("llama.cpp", llama_url, "sha", llama_sha))

    ort_url = _get("ONNXRUNTIME_GIT_URL")
    ort_tag = _get("ONNXRUNTIME_GIT_TAG")
    genai_url = _get("ONNXRT_GENAI_GIT_URL")
    genai_tag = _get("ONNXRT_GENAI_GIT_TAG")
    if ort_url and ort_tag:
        pins.append(FrameworkPin("onnxruntime", ort_url, "tag", ort_tag))
    if genai_url and genai_tag:
        pins.append(FrameworkPin("onnxruntime-genai", genai_url, "tag", genai_tag))

    mnn_url = _get("MNN_GIT_URL")
    mnn_tag = _get("MNN_GIT_TAG")
    if mnn_url and mnn_tag:
        pins.append(FrameworkPin("mnn", mnn_url, "tag", mnn_tag))

    # Mediapipe intentionally ignored for now (user request).
    return pins


def main() -> int:
    parser = argparse.ArgumentParser(description="Check whether pinned framework versions have newer upstream tags/commits.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd(), help="Repo root (default: cwd).")
    parser.add_argument("--build-dir", type=Path, default=Path("build"), help="Build dir for pin extraction.")
    parser.add_argument("--offline", action="store_true", help="Do not contact network; only print current pins.")
    parser.add_argument("--json", action="store_true", help="Emit JSON report.")
    args = parser.parse_args()

    repo_root: Path = args.repo_root.resolve()
    build_dir: Path = (repo_root / args.build_dir).resolve()

    pins = _read_framework_pins(repo_root, build_dir)

    updates: list[FrameworkUpdate] = []
    errors: list[str] = []

    if not args.offline:
        for pin in pins:
            try:
                if pin.pin_type == "tag":
                    latest = _latest_semver_tag(pin.repo_url)
                    if not latest:
                        errors.append(f"{pin.name}: could not determine latest tag from {pin.repo_url}")
                        continue
                    if _semver_key(latest) and _semver_key(pin.pin_value) and _semver_key(latest) > _semver_key(pin.pin_value):  # type: ignore[operator]
                        updates.append(
                            FrameworkUpdate(
                                name=pin.name,
                                pinned=pin.pin_value,
                                latest=latest,
                                latest_type="tag",
                                detail=f"newer semver tag available on {pin.repo_url}",
                            )
                        )
                elif pin.pin_type == "sha":
                    head = _remote_head_sha(pin.repo_url)
                    if not head:
                        errors.append(f"{pin.name}: could not determine remote HEAD from {pin.repo_url}")
                        continue
                    if head != pin.pin_value:
                        updates.append(
                            FrameworkUpdate(
                                name=pin.name,
                                pinned=pin.pin_value,
                                latest=head,
                                latest_type="head",
                                detail=f"remote HEAD differs (pinned SHA is behind HEAD)",
                            )
                        )
            except Exception as e:
                errors.append(f"{pin.name}: {e}")

    report = {
        "repo_root": str(repo_root),
        "build_dir": str(build_dir),
        "offline": args.offline,
        "pins": [pin.__dict__ for pin in pins],
        "updates": [u.__dict__ for u in updates],
        "errors": errors,
    }

    if args.json:
        print(json.dumps(report, indent=2))
        return 0 if not updates and not errors else (2 if updates else 1)

    print("Pinned frameworks:")
    for p in pins:
        print(f"- {p.name}: {p.pin_type}={p.pin_value}")

    if args.offline:
        print("\nOffline mode: skipping network checks.")
        return 0

    if errors:
        print("\nErrors:")
        for e in errors:
            print(f"- {e}")

    if updates:
        print("\nUpdates available:")
        for u in updates:
            print(f"- {u.name}: pinned {u.pinned} -> latest {u.latest} ({u.latest_type}); {u.detail}")
        print("\nRecommended next step:")
        print("- Use `skills/llm-framework-version-tracker/SKILL.md` to bump one framework at a time and run build+ctest.")
        return 2

    print("\nNo updates detected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

