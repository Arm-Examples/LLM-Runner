#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  framework_versions.sh [build-dir]

Reports pinned framework versions/commits and any CMake cache overrides.
EOF
}

if [[ $# -gt 1 ]]; then
  usage
  exit 2
fi

build_dir="${1:-build}"
python3 scripts/dev/framework_versions.py --build-dir "${build_dir}"

