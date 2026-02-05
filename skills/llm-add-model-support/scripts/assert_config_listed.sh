#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  assert_config_listed.sh <config-filename>

Checks that the given config filename appears in test/CMakeLists.txt.
Example:
  bash skills/llm-add-model-support/scripts/assert_config_listed.sh llamaTextConfig-phi-2.json
EOF
}

if [[ $# -ne 1 ]]; then
  usage
  exit 2
fi

cfg="$1"
file="test/CMakeLists.txt"

if [[ ! -f "${file}" ]]; then
  echo "Missing ${file}" >&2
  exit 1
fi

if grep -Fq "\"${cfg}\"" "${file}"; then
  echo "OK: ${cfg} is referenced in ${file}"
  exit 0
fi

echo "Missing: ${cfg} is not referenced in ${file}" >&2
echo "Add it under the correct LLM_FRAMEWORK branch (CONFIG_FILE_NAME list)." >&2
exit 1

