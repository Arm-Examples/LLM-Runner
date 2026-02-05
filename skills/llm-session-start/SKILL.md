---
name: llm-session-start
description: Run the standard “session start” checks for this repository (doctor checks + framework version report + optional upstream update check) and summarize actionable findings. Use at the start of a Codex session or before making changes to backends/models/benchmarks to confirm environment, wiring, and whether pinned frameworks have upstream updates.
---

# LLM session start

Codex CLI does not automatically run commands on repo initialization just because `AGENTS.md`/skills exist. This skill provides a one-command workflow you can invoke at the start of a session.

## Workflow

### 1) Run fast environment + wiring checks

```sh
bash skills/llm-session-start/scripts/start.sh build
```

This runs:
- `python3 scripts/dev/llm_doctor.py --build-dir <build-dir>`
- `python3 scripts/dev/framework_versions.py --build-dir <build-dir>`

### 2) Optionally check upstream for updates (network)

The upstream update check contacts Git remotes and requires network access:

```sh
bash skills/llm-session-start/scripts/start.sh build --network
```

Exit code behavior:
- `0`: no updates detected
- `2`: updates available
- `1`: error checking upstream

If updates are available, follow `skills/llm-framework-version-tracker/SKILL.md`.

## Notes

If you want this to run automatically before launching Codex, create a shell alias/wrapper outside the repo (see `skills/llm-session-start/references/wrapper-alias.md`).
