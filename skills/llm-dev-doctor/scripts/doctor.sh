#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  doctor.sh [build-dir]

Convenience wrapper around scripts/dev/llm_doctor.py.
EOF
}

if [[ $# -gt 1 ]]; then
  usage
  exit 2
fi

build_dir="${1:-build}"
python3 scripts/dev/llm_doctor.py --build-dir "${build_dir}"

