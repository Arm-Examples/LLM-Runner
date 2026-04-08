#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="bench_smoke.py",
        description="Smoke-checks that llm-bench-cli exists and its --help mentions --context.",
    )
    parser.add_argument("build_dir", help="e.g. build")
    args = parser.parse_args()

    build_dir = Path(args.build_dir)

    candidates = [
        build_dir / "bin" / "llm-bench-cli",
        build_dir / "bin" / "llm-bench-cli.exe",
    ]
    bench_bin = next((p for p in candidates if p.is_file()), None)
    if bench_bin is None:
        print(f"Missing executable: {candidates[0]}", file=sys.stderr)
        print(
            f"Build with: cmake --preset=native -B {build_dir} -DBUILD_BENCHMARK=ON && cmake --build {build_dir} --parallel",
            file=sys.stderr,
        )
        return 1

    result = subprocess.run([str(bench_bin), "--help"], capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        print(f"Failed: {bench_bin} --help (exit {result.returncode})", file=sys.stderr)
        return result.returncode

    print(f"OK: {bench_bin} --help")

    output = (result.stdout or "") + (result.stderr or "")
    if "--context" not in output:
        print(f"Expected {bench_bin} --help to mention --context", file=sys.stderr)
        return 1

    print(f"OK: {bench_bin} --help mentions --context")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
