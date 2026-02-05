#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  collect_debug_bundle.sh <build-dir> [out-dir]

Creates a tar.gz bundle with high-signal build/test logs and environment info for bug reports.
EOF
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 2
fi

build_dir="$1"
out_dir="${2:-.}"

if [[ ! -d "${build_dir}" ]]; then
  echo "Missing build dir: ${build_dir}" >&2
  exit 1
fi

mkdir -p "${out_dir}"
ts="$(date +%Y%m%d-%H%M%S)"
bundle_dir="$(mktemp -d)"
trap 'rm -rf "${bundle_dir}"' EXIT

{
  echo "date: $(date -Iseconds)"
  echo "uname: $(uname -a)"
  echo "cmake: $(cmake --version 2>/dev/null | head -n 1 || true)"
  echo "python: $(python3 --version 2>/dev/null || true)"
  echo "java: $(java -version 2>&1 | head -n 1 || true)"
  echo "JAVA_HOME: ${JAVA_HOME:-<unset>}"
} > "${bundle_dir}/env.txt"

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git rev-parse HEAD > "${bundle_dir}/git_head.txt" 2>/dev/null || true
  git status --porcelain=v1 > "${bundle_dir}/git_status.txt" 2>/dev/null || true
  git diff > "${bundle_dir}/git_diff.patch" 2>/dev/null || true
fi

cp -f "${build_dir}/CMakeCache.txt" "${bundle_dir}/" 2>/dev/null || true
cp -f "${build_dir}/CMakeFiles/CMakeOutput.log" "${bundle_dir}/" 2>/dev/null || true
cp -f "${build_dir}/CMakeFiles/CMakeError.log" "${bundle_dir}/" 2>/dev/null || true

if [[ -f "${build_dir}/Testing/Temporary/LastTest.log" ]]; then
  mkdir -p "${bundle_dir}/Testing/Temporary"
  cp -f "${build_dir}/Testing/Temporary/LastTest.log" "${bundle_dir}/Testing/Temporary/"
fi

# If a downloads log exists (gitignored), capture it too.
if [[ -f "download.log" ]]; then
  cp -f "download.log" "${bundle_dir}/" 2>/dev/null || true
fi

bundle="${out_dir}/llm-debug-bundle-${ts}.tar.gz"
tar -C "${bundle_dir}" -czf "${bundle}" .
echo "Wrote ${bundle}"

