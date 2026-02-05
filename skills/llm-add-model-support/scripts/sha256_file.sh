#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  sha256_file.sh <path>

Prints the SHA256 hash of a local file (useful for scripts/py/requirements.json entries).
EOF
}

if [[ $# -ne 1 ]]; then
  usage
  exit 2
fi

path="$1"
if [[ ! -f "${path}" ]]; then
  echo "Missing file: ${path}" >&2
  exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "${path}" | awk '{print $1}'
elif command -v shasum >/dev/null 2>&1; then
  shasum -a 256 "${path}" | awk '{print $1}'
else
  echo "No sha256 tool found (sha256sum or shasum)." >&2
  exit 1
fi

