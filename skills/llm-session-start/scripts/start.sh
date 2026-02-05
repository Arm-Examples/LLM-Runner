#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  start.sh [build-dir] [--network]

Runs:
  - scripts/dev/llm_doctor.py
  - scripts/dev/framework_versions.py
Optionally:
  - scripts/dev/check_framework_updates.py (requires network)

Examples:
  bash skills/llm-session-start/scripts/start.sh build
  bash skills/llm-session-start/scripts/start.sh build --network
EOF
}

build_dir="build"
network="0"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    --network) network="1"; shift ;;
    *)
      if [[ "${build_dir}" != "build" ]]; then
        usage
        exit 2
      fi
      build_dir="$1"
      shift
      ;;
  esac
done

python3 scripts/dev/llm_doctor.py --build-dir "${build_dir}"
echo
python3 scripts/dev/framework_versions.py --build-dir "${build_dir}"

if [[ "${network}" == "1" ]]; then
  echo
  python3 scripts/dev/check_framework_updates.py --build-dir "${build_dir}"
fi

