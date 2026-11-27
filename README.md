# Ethernauta

A JUCE + C++ drone synthesizer. Each drone voice has a waveform selector, a latch switch (the voice
keeps sounding until toggled off), a gain fader and a pitch control.

Status: step 1. One fully working voice, architected so that six voices are a loop, not a rewrite.

License: undecided (no LICENSE file yet).

## Structure

| Path | Contents |
|---|---|
| `Source/core` | `ethernauta_core`: JUCE-free, real-time-safe C++17 static library. `Oscillator` (PolyBLEP sine/saw/square, triangle), `ParamSmoother`, `LatchGate`, `DroneVoice`, `VoiceParams` |
| `Source/plugin` | `EthernautaPlugin` (VST3 + Standalone): processor, editor, `VoicePanel`, parameter layout |
| `tests` | Catch2 v3 tests for the core, plus a guard that the core links no JUCE |
| `cmake` | Warning policy, pinned JUCE fetch, JUCE-link guard script |
| `docs/research` | Research notes |
| `troubleshooting` | Solved-bug knowledge base |
| `.claude/skills`, `AGENTS.md` | Agent skills and conventions |

Design notes:

- `DroneVoice::process()` **adds** into the output buffer (clear once, loop over voices).
- Gain is in dB; at or below -60 dB the voice is exactly muted. Pitch is a fractional MIDI note plus cents (default A2 = 110 Hz).
- Block-size independent: any block split renders bit-identical audio.
- Parameter IDs are stable strings per voice: `v1_wave`, `v1_latch`, `v1_gain`, `v1_pitch` (then `v2_...`).

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

Host validation: `pluginval --strictness-level 10 --validate <path to .vst3>`.

Other options: `-DETHERNAUTA_JUCE_DIR=<path>`, `-DETHERNAUTA_JUCE_FETCH=OFF`, `-DETHERNAUTA_SANITIZE=ON` (clang/gcc).

## Roadmap

1. One voice (this step).
2. Six independent voices (loop over `std::array<DroneVoice, 6>`; raise `kNumVoices`).
3. Autotune / harmonic tuning.
4. Three-voice choirs.
5. Linking voices to replicate one voice's setup.
6. Release hardening: pluginval level 10, license decision, installers.
