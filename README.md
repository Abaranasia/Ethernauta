# Ethernauta

A JUCE + C++ drone synthesizer with six independent drone voices. Each voice has a waveform selector,
a latch switch (the voice keeps sounding until toggled off), a gain fader and a pitch control.

Status: step 4, slice 1. Six independent voices (`Voice 1`..`Voice 6`), click-free waveform switching, an
output headroom stage, a shared-root tuning engine (free, 12-TET, just, degree, choir) with glide, snap
controls, choir tuning (triads from the shared root) and plugin-layer tests. No voice linking, modulation
or roughness model yet.

License: undecided (no LICENSE file yet).

## Structure

| Path | Contents |
|---|---|
| `Source/core` | `ethernauta_core`: JUCE-free, real-time-safe C++17 static library. `Oscillator` (PolyBLEP sine/saw/square, triangle; 5 ms waveform crossfade), `ParamSmoother`, `LatchGate`, `DroneVoice`, `VoiceParams`, `MasterStage`, `Tuning` (math + just table), `Choir` (triad ratios), `TuningEngine` |
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
- Parameter IDs are stable strings per voice: `v1_wave`, `v1_latch`, `v1_gain`, `v1_pitch`, `v1_tuning`, `v1_degree`, `v1_octave` ... `v6_*`, plus `v1_snap` ... `v6_snap` and the globals `root`, `snap_hysteresis`, `choir1_chord`, `choir2_chord` (52 parameters). Parameter order is append-only because hosts may automate by index: `v1..v6` wave/latch/gain/pitch first (the original layout), then `v1..v6` tuning/degree/octave, then `root`, then `v1..v6` snap and `snap_hysteresis`, then `choir1_chord`, `choir2_chord`; new parameters are appended, never inserted. Defaults: sine, unlatched, -12 dB; pitches form an open A-major stack (A2 E3 A3 C#4 E4 A4); tuning Free.
- Headroom: the summed voices pass through a fixed -6 dB trim and `tanh`, so six latched full-gain voices stay within [-1, 1]; silence stays exactly 0.
- A waveform change while sounding crossfades over 5 ms (no click); while idle it switches instantly. A gain change while idle (or at latch) never ramps.

## Tuning engine

All voices are tuned from ONE shared root (never neighbour to neighbour), so just intervals cannot
accumulate comma drift. The root is the global `root` parameter (MIDI note 12..96, whole notes,
default 45 = A2 = 110 Hz). Each voice picks a mode (`v{n}_tuning`):

| Mode | Target frequency of the voice |
|---|---|
| Free (default) | the voice's own `pitch` parameter, as before; root, degree and octave are ignored |
| 12-TET | `pitch` snapped to the nearest equal-tempered semitone relative to the root, plus `octave` octaves |
| Just | `pitch` snapped to the nearest entry of the 16-entry just scale relative to the root (octave kept), plus `octave` octaves |
| Degree | `root * ratio(degree) * 2^octave`; `pitch` is ignored |
| Choir | `root * ratio(chord, member) * 2^octave`; `pitch` is ignored (see below) |

- `v{n}_degree` indexes the table in `Tuning.h`: 0..15 are the octave-reduced 5-/7-limit intervals sorted by cents (`1/1` ... `15/8`), 16 is `2/1`, 17..32 are harmonic partials 1..16. The order is frozen; entries may only be appended.
- `v{n}_octave` is an integer -3..+3 added to the tuned result.
- Snapping uses 3 cents of hysteresis: a voice leaves its grid point only when another is more than 3 cents closer, so jitter at a midpoint cannot flap it. A root change re-snaps Free-pitch voices on the new grid; Degree voices keep exact ratios to the root and transpose with it.
- Frequency changes glide: while a voice sounds, its frequency moves to the new target with a one-pole of 80 ms in the log (cents) domain, so retuning does not click. An idle voice jumps straight to its target. The glide also applies to the plain pitch knob. It advances per sample, so rendering stays bit-identical for any block size.
- The pitch dial stays enabled in every mode (it is the value to tune in 12-TET and Just); only Degree and Choir ignore it.
- State schema 2 added these parameters; state saved by schema 1 loads with the tuning parameters at their defaults.
- Out of scope for this slice: root cents, voice linking, modulation, roughness model.

### Choir tuning

A choir is three voices tuned as a triad from the one shared root (never neighbour to neighbour, so no comma drift). Voices 1-3 are choir 1 and voices 4-6 choir 2; a voice's member is `voice % 3` (0 = root, 1 = third, 2 = fifth). Set a voice's tuning to `Choir` (the last `v{n}_tuning` choice) and pick the chord of its choir with the global `choir1_chord` / `choir2_chord` (editor: "Choir 1" / "Choir 2"):

| Chord | Root | Third | Fifth |
|---|---|---|---|
| Major (default) | 1/1 | 5/4 | 3/2 |
| Minor | 1/1 | 6/5 | 3/2 |
| Septimal Minor | 1/1 | 7/6 | 3/2 |
| Supermajor | 1/1 | 9/7 | 3/2 |

- `v{n}_octave` still transposes each voice; `pitch` is ignored. The chord order is frozen (`Source/core/include/ethernauta/Choir.h`); entries may only be appended.
- State schema 4 added these parameters; older state loads with both chords at Major.

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
3. Autotune / harmonic tuning (slice 1 done: shared root, modes, glide).
4. Three-voice choirs (slice 1 done: triads from the shared root; drift, timbre/formants and linking later).
5. Linking voices to replicate one voice's setup.
6. Release hardening: pluginval level 10 in CI, CPU budget test, license decision, installers.

Testing status (what is covered and what is pending) is tracked in `.claude/skills/juce-plugin-testing/SKILL.md`.
