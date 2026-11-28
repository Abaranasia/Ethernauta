---
name: juce-plugin-testing
description: "Trigger: pluginval, JUCE tests, unit test, plugin validation, CI, DSP test. Apply JUCE plugin testing workflow: pluginval host validation and DSP unit tests."
license: Apache-2.0
metadata:
  author: "Abaranasia"
  version: "1.0"
---

## Activation Contract
Apply when setting up, writing, or running tests for this JUCE plugin: unit tests for DSP/processor logic, or host-compatibility validation.

## Hard Rules
- **Validate the built plugin with pluginval** (Tracktion/pluginval) at strictness level 10 before calling a build release-ready: `pluginval --strictness-level 10 --validate <path-to-vst3>`. It catches threading, parameter-automation, and state-restore bugs unit tests miss.
- **Unit-test DSP logic, not JUCE itself.** The JUCE-free core (`ethernauta_core`) is tested with Catch2 v3 (fetched by `tests/CMakeLists.txt`), which this skill permits because the project already depends on it. Do not add a second test framework; use `juce::UnitTest` only for JUCE-dependent plugin code, if ever needed.
- **Test parameter round-trips**: for every parameter, assert `setStateInformation(getStateInformation())` restores the same value.
- **Test at multiple block sizes and sample rates** (e.g. 1, 7, 512, 4096 samples; 44100/48000/96000 Hz) — DSP tested at only one block size hides off-by-one and buffer-boundary bugs.
- **Test feedback stability on any regenerative path**  (feedback delay, resonator, filter in a loop). Process a sustained signal for hundreds of blocks and assert peak amplitude stays bounded; this is the one test category a plain impulse-response check won't catch.
- **Test silent input on every DSP component that has internal state** (oscillator phase, smoothers, gates, delay lines) — assert output stays near the noise floor. Catches uninitialized/uncleared buffers and denormal build-up that pluginval won't always surface.
- **Never assert on real-time timing behavior via wall-clock in unit tests** — that's flaky. Test correctness (output values, state), not speed. The exception is a *relative* CPU budget check (real-time factor = processing time / audio time), which is a correctness property for a real-time system, not a timing flake — a single drone voice should stay far below 0.01 real-time factor in a Release build.

## Decision Gates
| Situation | Do this |
|---|---|
| New DSP algorithm added | Add a Catch2 test exercising known input→output cases (impulse, silence, DC, Nyquist) |
| DSP component has feedback/regeneration | Add the sustained-input stability test before wiring it into the voice/engine, not after |
| New parameter added | Add a state round-trip test and a pluginval automation pass |
| Changing an existing parameter's ID, range, or type | Treat as breaking once any preset/session exists — see the equivalent gate in `juce-plugin-dev` |
| Preparing a release build | Run `pluginval --strictness-level 10` against every plugin format target (VST3, Standalone) before tagging |

## Execution Steps
1. Locate the Catch2 test target (`tests/`, run via `ctest`); create a `juce::UnitTestRunner` target only for JUCE-dependent code.
2. For DSP changes, write the failing test first, then implement.
3. Before marking a build release-ready, run pluginval against the actual built binary, not just unit tests.

## Output Contract
Before reporting testing work done, confirm: new DSP/parameters have unit tests, state round-trip is covered, any regenerative path has a bounded-output stability test, pluginval has been run against the built plugin binary.


## Status

Which rules are met in this repo today (update when it changes).

| Rule | State |
|---|---|
| DSP unit tests for the core (Catch2) | Satisfied: oscillator, smoother, gate, voice, master stage, tuning math/table/engine, voice glide |
| Multiple block sizes (1, 7, 512, 4096) and sample rates (44100/48000/96000) | Satisfied in core and plugin tests; block-size output is bit-identical |
| Silent-state tests (idle voice, release to exact silence, no latched voice) | Satisfied |
| Parameter state round-trip for every parameter of every voice | Satisfied (`tests/plugin`, opt-in `ETHERNAUTA_BUILD_PLUGIN=ON`) |
| Malformed/foreign state, stable parameter IDs, layout defaults/ranges | Satisfied (`tests/plugin`), including the tuning parameters and schema-1 migration |
| Output bounded with six voices at full gain | Satisfied (core and plugin tests) |
| Feedback-stability test | Not applicable yet: no regenerative path exists |
| Relative CPU budget (real-time factor, Release build) | Pending: no test written |
| pluginval strictness 10 on the VST3 | Run manually before release only; not automated in CI |
| pluginval on the Standalone target | Not applicable (pluginval validates plugin formats, not the Standalone app) |
| Plugin-layer tests in CI | Informational job only (JUCE build on CI is not a blocking gate) |
