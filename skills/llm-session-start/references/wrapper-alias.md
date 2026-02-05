<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Optional: wrapper alias to run checks before Codex

Codex CLI does not (currently) support “auto-run this command on repo open” based on `AGENTS.md`/skills.

If you want an automatic reminder/check before launching Codex, create a shell function/alias in your shell rc file (outside the repo).

Example (bash/zsh):

```sh
codex-llm() {
  local repo="/path/to/large-language-models"
  (cd "$repo" && bash skills/llm-session-start/scripts/start.sh build --network) || return $?
  command codex "$@"
}
```

Then run `codex-llm` instead of `codex` for this repo.

