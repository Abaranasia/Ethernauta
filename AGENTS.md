# Agent Skills — Ethernauta (JUCE Drone Synthesizer)

Project-scoped skills under `.claude/skills/`, applied automatically by trigger context.

| Skill | Applies to | Covers |
|---|---|---|
| [juce-plugin-dev](.claude/skills/juce-plugin-dev/SKILL.md) | `PluginProcessor`, `PluginEditor`, DSP classes, `processBlock`, APVTS, plugin state | Real-time audio-thread safety, parameter management, state persistence, latency/bypass handling |
| [juce-plugin-testing](.claude/skills/juce-plugin-testing/SKILL.md) | Test setup, DSP unit tests, release validation | Catch2 tests for the JUCE-free core, pluginval host-compatibility validation |
| [troubleshooting-docs](.claude/skills/troubleshooting-docs/SKILL.md) | Right after a non-trivial JUCE/DSP/build bug is confirmed fixed | Captures the fix as a categorized, schema-validated doc under `troubleshooting/` so it's searchable next session |

## Project docs

- [README.md](README.md) — what Ethernauta is, build/test commands, source layout, roadmap
- [troubleshooting/](troubleshooting/README.md) — solved-bug knowledge base (schema in `.claude/skills/troubleshooting-docs/schema.yaml`)
- `docs/research/` — research notes (written separately; not part of the scaffold)

## Architecture rules

- `Source/core` (`ethernauta_core`) is JUCE-free C++17 and real-time safe (no allocation, locks or exceptions in process paths). It is guarded by a configure-time check and the `core_links_no_juce` test.
- `Source/plugin` is the only place that may use JUCE. Parameter IDs are stable strings (`v<N>_wave`, `v<N>_latch`, `v<N>_gain`, `v<N>_pitch`) and must never change once shipped.
- Write the failing test first (strict TDD) for every change to the core.
