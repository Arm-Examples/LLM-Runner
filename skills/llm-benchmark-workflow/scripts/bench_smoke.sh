#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bench_smoke.sh <build-dir>

Checks:
  - build/bin/arm-llm-bench-cli exists
  - arm-llm-bench-cli prints help

Example:
  bash skills/llm-benchmark-workflow/scripts/bench_smoke.sh build
EOF
}

if [[ $# -ne 1 ]]; then
  usage
  exit 2
fi

build_dir="$1"
bin="${build_dir}/bin/arm-llm-bench-cli"

if [[ ! -x "${bin}" ]]; then
  echo "Missing executable: ${bin}" >&2
  echo "Build with: cmake --preset=native -B ${build_dir} -DBUILD_BENCHMARK=ON && cmake --build ${build_dir}" >&2
  exit 1
fi

"${bin}" --help >/dev/null
echo "OK: ${bin} --help"

