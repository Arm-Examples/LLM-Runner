---
name: llm-dev-doctor
description: Run fast “doctor” checks and generate a debug bundle for this repository (toolchain versions, CMake cache, test config wiring, JNI availability, and build logs). Use when diagnosing local build/CTest failures, preparing a bug report, or validating a dev environment before changing backends/models/benchmarks.
---

# LLM dev doctor

This skill uses lightweight scripts to quickly identify common environment and wiring issues without re-downloading models or rebuilding from scratch.

## Workflow

### 1) Run doctor checks

From repo root:

```sh
python3 scripts/dev/llm_doctor.py --build-dir build
```

If there is no `build/` yet, you can still validate test wiring for a specific backend:

```sh
python3 scripts/dev/llm_doctor.py --build-dir build --llm-framework llama.cpp
```

### 2) Create a debug bundle for bug reports

```sh
bash scripts/dev/collect_debug_bundle.sh build .
```

This produces `llm-debug-bundle-<timestamp>.tar.gz` with:
- environment versions (`env.txt`)
- CMake logs/cache (`CMakeCache.txt`, `CMakeOutput.log`, `CMakeError.log`)
- last CTest log if present
- git head/status/diff (if inside a git worktree)

### 3) Interpret common FAILs

- `FAIL: CMakeCache missing`: run `cmake --preset=native -B build` first.
- `FAIL: java/javac missing`: retry with `-DBUILD_JNI_LIB=OFF` or install/configure a JDK (set `JAVA_HOME`).
- `FAIL: config files exist / JSON valid`: fix `model_configuration_files/*.json` or the `test/CMakeLists.txt` config list.

## Notes

See `skills/llm-dev-doctor/references/what-it-checks.md` for details.

`CMakeError.log` / `CMakeOutput.log` may be missing on successful configurations; this is informational, not an error.

### scripts/
Executable code (Python/Bash/etc.) that can be run directly to perform specific operations.

**Examples from other skills:**
- PDF skill: `fill_fillable_fields.py`, `extract_form_field_info.py` - utilities for PDF manipulation
- DOCX skill: `document.py`, `utilities.py` - Python modules for document processing

**Appropriate for:** Python scripts, shell scripts, or any executable code that performs automation, data processing, or specific operations.

**Note:** Scripts may be executed without loading into context, but can still be read by Codex for patching or environment adjustments.

### references/
Documentation and reference material intended to be loaded into context to inform Codex's process and thinking.

**Examples from other skills:**
- Product management: `communication.md`, `context_building.md` - detailed workflow guides
- BigQuery: API reference documentation and query examples
- Finance: Schema documentation, company policies

**Appropriate for:** In-depth documentation, API references, database schemas, comprehensive guides, or any detailed information that Codex should reference while working.

### assets/
Files not intended to be loaded into context, but rather used within the output Codex produces.

**Examples from other skills:**
- Brand styling: PowerPoint template files (.pptx), logo files
- Frontend builder: HTML/React boilerplate project directories
- Typography: Font files (.ttf, .woff2)

**Appropriate for:** Templates, boilerplate code, document templates, images, icons, fonts, or any files meant to be copied or used in the final output.

---

**Not every skill requires all three types of resources.**
