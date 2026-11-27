# Troubleshooting knowledge base

Solved, non-trivial JUCE / DSP / build problems live here so the next session does not re-debug them.
It is empty for now; the structure is created on demand.

- One problem per file: `troubleshooting/<category>/<sanitized-symptom>-<YYYYMMDD>.md`.
- Frontmatter must validate against [`.claude/skills/troubleshooting-docs/schema.yaml`](../.claude/skills/troubleshooting-docs/schema.yaml); the `problem_type` to directory mapping is defined there.
- Categories: `build-failures`, `runtime-issues`, `api-usage`, `validation-problems`, `gui-issues`, `dsp-issues`, `parameter-issues`, `performance-issues`, `thread-safety`.
- Search before writing (`rg "<symptom>" troubleshooting/`) and cross-link related docs instead of duplicating.
- Workflow: see the [troubleshooting-docs skill](../.claude/skills/troubleshooting-docs/SKILL.md).
