#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  run_build_and_ctest.sh <preset> <build-dir> [-- <cmake-configure-args...>]

Examples:
  bash skills/llm-build-and-ctest/scripts/run_build_and_ctest.sh native build
  bash skills/llm-build-and-ctest/scripts/run_build_and_ctest.sh native build -- -DBUILD_JNI_LIB=OFF
  bash skills/llm-build-and-ctest/scripts/run_build_and_ctest.sh native build -- -DLLM_FRAMEWORK=mnn
EOF
}

if [[ $# -lt 2 ]]; then
  usage
  exit 2
fi

preset="$1"
build_dir="$2"
shift 2

cmake_args=()
if [[ $# -gt 0 ]]; then
  if [[ "${1:-}" != "--" ]]; then
    usage
    exit 2
  fi
  shift
  cmake_args=("$@")
fi

cmake --preset="${preset}" -B "${build_dir}" "${cmake_args[@]}"
cmake --build "${build_dir}"
ctest --test-dir "${build_dir}" --output-on-failure

