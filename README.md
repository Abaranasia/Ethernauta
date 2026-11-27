# Ethernauta

A JUCE + C++ drone synthesizer with six independent drone voices. Each voice has a waveform selector,
a latch switch (the voice keeps sounding until toggled off), a gain fader and a pitch control.

Status: step 2. Six independent voices (`Voice 1`..`Voice 6`), click-free waveform switching, an
output headroom stage, and plugin-layer tests. No tuning engine, choirs or voice linking yet.

License: undecided (no LICENSE file yet).

## Structure

| Path | Contents |
|---|---|
| `Source/core` | `ethernauta_core`: JUCE-free, real-time-safe C++17 static library. `Oscillator` (PolyBLEP sine/saw/square, triangle; 5 ms waveform crossfade), `ParamSmoother`, `LatchGate`, `DroneVoice`, `VoiceParams`, `MasterStage` |
| `Source/plugin` | `EthernautaPlugin` (VST3 + Standalone): processor, editor, `VoicePanel`, parameter layout |
| `tests` | Catch2 v3 tests for the core, plus a guard that the core links no JUCE |
| `tests/plugin` | Opt-in Catch2 tests of the real processor, parameter layout, state and editor (needs `ETHERNAUTA_BUILD_PLUGIN=ON`) |
| `cmake` | Warning policy, pinned JUCE fetch, JUCE-link guard script |
| `docs/research` | Research notes |
| `troubleshooting` | Solved-bug knowledge base |
| `.claude/skills`, `AGENTS.md` | Agent skills and conventions |

Design notes:

- `DroneVoice::process()` **adds** into the output buffer (clear once, loop over voices).
- Gain is in dB; at or below -60 dB the voice is exactly muted. Pitch is a fractional MIDI note plus cents (default A2 = 110 Hz).
- Block-size independent: any block split renders bit-identical audio.
- Six voices: `kNumVoices` in `Source/plugin/ParameterLayout.h` is the single count used by the layout, processor and editor.
- Parameter IDs are stable strings per voice: `v1_wave`, `v1_latch`, `v1_gain`, `v1_pitch` ... `v6_*` (24 parameters). Defaults: sine, unlatched, -12 dB; pitches form an open A-major stack (A2 E3 A3 C#4 E4 A4).
- Headroom: the summed voices pass through a fixed -6 dB trim and `tanh`, so six latched full-gain voices stay within [-1, 1]; silence stays exactly 0.
- A waveform change while sounding crossfades over 5 ms (no click); while idle it switches instantly. A gain change while idle (or at latch) never ramps.

## Build and test

Default build: core and tests only, no JUCE needed (Catch2 is fetched at a pinned commit).

```
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Plugin build (opt-in, use a separate directory). JUCE is taken from `ETHERNAUTA_JUCE_DIR`
(default `../../JUCE`, relative to this repo); if absent, pinned JUCE 9.0.0 is fetched.

```
cmake -S . -B build-plugin -DETHERNAUTA_BUILD_PLUGIN=ON
cmake --build build-plugin --config Debug --target EthernautaPlugin_Standalone EthernautaPlugin_VST3
```

Plugin-layer tests (opt-in, Catch2 linked against the plugin's shared-code target, so they use the real
`AudioProcessor`; Catch2 is already a dependency, no second framework). Building them also builds the
core tests in the same directory:

```
cmake --build build-plugin --config Debug
cmake --build build-plugin --config Debug --target ethernauta_plugin_tests
ctest --test-dir build-plugin -C Debug --output-on-failure
```

Using a local JUCE (`ETHERNAUTA_JUCE_DIR`) bypasses the pinned download and prints a CMake warning.

Host validation: `pluginval --strictness-level 10 --validate <path to .vst3>`.

Other options: `-DETHERNAUTA_JUCE_DIR=<path>`, `-DETHERNAUTA_JUCE_FETCH=OFF`, `-DETHERNAUTA_SANITIZE=ON` (clang/gcc).

## Roadmap

1. One voice (done).
2. Six independent voices (done).
3. Autotune / harmonic tuning.
4. Three-voice choirs.
5. Linking voices to replicate one voice's setup.
6. Release hardening: pluginval level 10 in CI, CPU budget test, license decision, installers.

Testing status (what is covered and what is pending) is tracked in `.claude/skills/juce-plugin-testing/SKILL.md`.
