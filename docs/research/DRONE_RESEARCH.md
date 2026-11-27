# Drone Synthesis Research for Ethernauta

Status: research written 2026-11-27.
Scope: a 6-voice JUCE/C++ drone synth (per-voice waveform, latch, gain now; auto-tuned harmonic voices, 3-voice choir groups and voice linking later).
Conventions: every factual claim has a link in the Sources section (S-numbers). **[UNVERIFIED]** marks claims I could not confirm from a fetched primary source. **[DERIVED]** marks math or design I worked out myself (standard DSP/music-theory arithmetic, not taken from a source). Cent values below were computed by me from `1200*log2(ratio)`.

---

## 1. Open-source projects worth studying

### 1.1 Licence summary (matters for reuse)

| Project | Licence (verified) | Language | Notes |
|---|---|---|---|
| Vital | GPL-3.0; author offers separate licensing for proprietary use [S1] | C++ (README does not list; JUCE-based per my knowledge) [UNVERIFIED] | Spectral-warping wavetable synth |
| Surge XT | GPL-3.0 [S2] | C++ (CMake, "modern C++ compiler") [S2] | 12 LFOs (6 voice, 6 scene), many oscillator types [S3] |
| Cardinal (VCV Rack as plugin) | GPLv3+; all bundled modules must be GPLv3+ compatible [S4] | not stated on page | Modular; useful as a patching-idea source |
| ZynAddSubFX | GPL-2.0-or-later [S5] | C++ (page does not state) [UNVERIFIED] | ADsynth, SUBsynth, PADsynth engines |
| Odin 2 | GPLv3 (font under OFL-1.1) [S6] | C++ / CMake | VST3/CLAP/AU/LV2 |
| Helm | GPL-3.0; repo archived 2025-02-08 [S7] | C++ with JUCE (folders present) [S7] | Up to 15 unison voices per oscillator, formant filter |
| Dexed | GPL-3.0; bundled msfa engine is Apache-2.0 [S8] | C++ with JUCE [S8] | DX7 FM; useful for sine/FM drone ideas |
| SuperCollider | GPL-3.0 [S9] | C++17 (+ sclang) | Not embeddable in a closed plugin |
| Csound | LGPL-2.1 [S10] | C | LGPL, still not a drop-in for a JUCE plugin |
| Pure Data | Licence type not stated on README; points to LICENSE.txt [S11]. Commonly described as BSD-style [UNVERIFIED] | C | Patch-level ideas only |
| PADsynth reference code | Public domain per ZynAddSubFX docs [S12] | C++ | Reusable algorithm, best reuse candidate |
| JUCE itself | AGPLv3 or commercial [S13]; JUCE 8 tiers: Starter free up to $20k revenue, Indie up to $300k ($40/mo or $800), Pro unlimited ($175/mo or $3,500) [S14] | C++ | Current `master` LICENSE.md refers to a "JUCE 9" EULA [S15]; check which version you pin |

### 1.2 Licence implications (not legal advice)

- Every serious synth above is GPL/AGPL-family. Copying or porting their code into Ethernauta makes Ethernauta a derivative: it must be released under a compatible licence (GPLv3 or AGPLv3 with source) and cannot be closed-source or sold under a JUCE commercial licence. [DERIVED from S1, S2, S13; GPL terms are standard, but read the licences or ask counsel.]
- If Ethernauta uses JUCE under AGPLv3, GPLv3 code can be combined with it. GPL-2.0-or-later code (ZynAddSubFX) can be taken under GPLv3. [DERIVED, general licence knowledge, not checked against a source here.]
- If Ethernauta ever ships closed-source under a JUCE commercial licence, treat all of these as **inspiration only**: read for ideas, then reimplement from the published algorithms (Section 2, Section 3) without copying code. Vital's author explicitly sells proprietary licences [S1]; that is the only route to reuse its code in closed software.
- Safest reusable pieces: PADsynth algorithm (public-domain reference, S12/S16), published papers' formulas, and your own code.

### 1.3 What is architecturally worth learning

| Idea | Where to look | Why it matters for drones |
|---|---|---|
| Unison with detune and stereo spread | Helm (up to 15 unison voices) [S7]; Surge XT "Unison Voices" parameter [S3]; supersaw analysis: detune curve is non-linear, saw oscillators plus a high-pass on the sum [S17, secondary summary, coefficients [UNVERIFIED]] | Slow beating between near-unison voices is the core "alive" drone texture. Use a non-linear detune law, not linear |
| Additive / spectral engine | ZynAddSubFX ADsynth/SUBsynth/PADsynth [S5] | A drone does not need note-rate pitch change, so expensive spectral methods are viable |
| PADsynth (wavetable via one inverse FFT) | ZynAddSubFX docs and wikibook [S12, S16, S18]; Csound GENpadsynth [S19] | Each harmonic is spread into a band whose width grows with harmonic number; random phases; the result is a perfectly looped table. Gives choir/ensemble-like smooth timbre with zero detuned oscillators |
| Wavetable / window oscillators and many oscillator types | Surge XT list: Classic, Modern, Wavetable, Window, Sine, FM2, FM3, String, Twist, Alias, S&H Noise [S3] | Candidate per-voice waveform selector content |
| Modulation sources including smooth noise | Surge XT LFO shapes: Noise (smooth), S&H, Envelope, Step Seq, MSEG, Formula (Lua) [S3]; Vital reportedly uses Perlin noise as a modulator (forum comment only) [S20] | Non-repeating slow modulation is what separates a drone from a static tone |
| Filter feedback | Surge XT Feedback control [S3] | Self-resonance adds slowly moving harmonics |
| Formant filter | Helm effects list [S7]; Csound vowel data [S21] | Choir groups (Section 4.4) |
| Modular patch thinking | Cardinal/VCV Rack [S4], Pure Data, SuperCollider, Csound | Prototype ideas quickly in a patch before writing C++ |

Gaps in what I could verify: I did not read the Vital, Surge or Odin source trees, so statements about their internal implementations (unison algorithms, modulation routing code) are not verified. The README-level facts above are.

---

## 2. Tuning for harmonic results

### 2.1 Concepts and sources

- **Plomp and Levelt (1965):** consonance of sine-tone intervals depends on critical bandwidth. Frequency differences wider than the critical bandwidth are consonant; the most dissonant interval is about a quarter of the bandwidth [S22]. This is the perceptual basis for "beating is the roughness we care about".
- **Sethares, "Tuning, Timbre, Spectrum, Scale":** principle of local consonance. A timbre and a scale are related if the timbre's dissonance curve has local minima at the scale's pitches. For harmonic timbres the minima fall near simple ratios (3:2, 4:3, 5:4). You can derive a scale from a timbre or a timbre from a scale [S23]. Stretched partials: `f_j = f * A^(log2 j)` (A = 2 gives the harmonic series). A timbre with n partials has at most `2n(n-1)` local minima [S23]. Book text is at S24 (a third-party hosted PDF; I did not read it).
- **Partch:** just intonation with an 11-limit tuning; each ratio has an otonality and a utonality identity [S25]. Tonality diamond: the n-limit diamond holds the ratios between 1 and 2 whose odd part is at most n; the 5-limit diamond has 7 members, 7-limit has 13, 11-limit has 29, 15-limit has 49 [S26].
- **La Monte Young:** The Well-Tuned Piano uses 7-limit just intonation built from harmonics 2, 3, 4, 6, 7 (avoids 5); lattice of 3/2 and 7/4 steps; example ratios relative to E-flat = 1/1: 3/2, 9/8, 7/4, 21/16, 63/32, 49/32 [S27]. Dream House is a sustained sine-tone drone installation; a recent version uses 32 sine tones [S28]. A Kyle Gann essay (secondary summary) says 35 sine tones over 10 octaves, 20 packed into one octave band [S29, [UNVERIFIED] detail]. I did not find Dream House's exact frequency list.
- **Euler-Fokker genus / lattice:** pitch sets generated by factors 3 and 5 (optionally 7+) laid out in a 2D/3D grid; the number of pitches = product of (exponent + 1) [S30]. I could not find a source for the "Euler lattice / tonnetz coordinate" definition itself; the exponent-vector representation in 2.3 is my own working design [DERIVED].
- **Adaptive just intonation:** a published scheme tunes chords toward ideal ratios and falls back to a tempered compromise when a chord cannot be fully just; JI always refers to a keynote [S31]. This is the closest published relative of "autotune free-running oscillators to ratios of a root".

### 2.2 Formulas usable in C++

Standard, verified for cents/ratio [S32]:

```cpp
inline double ratioToCents(double r)  { return 1200.0 * std::log2(r); }
inline double centsToRatio(double c)  { return std::exp2(c / 1200.0); }   // 2^(c/1200)
inline double hzToCentsRel(double f, double root) { return 1200.0 * std::log2(f / root); }
inline double midiToHz(double n)      { return 440.0 * std::exp2((n - 69.0) / 12.0); }
```

For per-sample or per-block frequency, prefer `exp2` of a smoothed *cents* value; smooth in the log domain, never in Hz. [DERIVED]

Beating between near-unison or near-ratio voices [DERIVED, standard]:
- Two partials at f1 and f2 beat at `|f1 - f2|` Hz.
- If voices are meant to be a ratio p:q (f_hi/f_lo = p/q), the coinciding partials are harmonic p of the low voice and harmonic q of the high voice, so beat rate = `|q*f_hi - p*f_lo|`.
- Example: root 110 Hz, 12-TET fifth = 164.8135 Hz vs just 3/2 = 165 Hz. Partials: 2 * 164.8135 = 329.627 vs 3 * 110 = 330, so beat = 0.373 Hz. This is inherent equal-temperament drift; with just ratios the beat is 0, i.e. the voices lock and the chord sounds "still". An intentional detune of a few cents gives slow, controllable beating.
- Detune of `c` cents at frequency f gives a beat of about `f * (2^(c/1200) - 1)` Hz. At 220 Hz, 3 cents is roughly 0.38 Hz. Use this to choose "chorus-like" vs "frozen" detune targets.

### 2.3 Just ratios table (computed)

| Interval | Ratio | Cents | Deviation vs 12-TET (cents) |
|---|---|---|---|
| Unison | 1/1 | 0 | 0 |
| Syntonic comma | 81/80 | 21.51 [S32] | n/a |
| Minor second (diatonic semitone) | 16/15 | 111.73 | +11.73 |
| Major second | 9/8 | 203.91 | +3.91 |
| Minor seventh-ish whole tone | 10/9 | 182.40 | -17.60 |
| Septimal minor third | 7/6 | 266.87 | -33.13 |
| Minor third | 6/5 | 315.64 | +15.64 |
| Major third | 5/4 | 386.31 [S32] | -13.69 |
| Septimal major third | 9/7 | 435.08 [S27] | +35.08 |
| Fourth | 4/3 | 498.04 | -1.96 |
| Septimal tritone | 7/5 | 582.51 | -17.49 |
| Fifth | 3/2 | 701.96 | +1.96 |
| Minor sixth | 8/5 | 813.69 | +13.69 |
| Major sixth | 5/3 | 884.36 | -15.64 |
| Harmonic seventh | 7/4 | 968.83 | -31.17 |
| Minor seventh | 9/5 | 1017.60 | +17.60 |
| Major seventh | 15/8 | 1088.27 | -11.73 |
| Octave | 2/1 | 1200 | 0 |

Compute them at startup from the rational, do not hard-code cents. Chord stacks from a root (all over the root, octave-reduced as needed):

- Otonal (harmonic series) chord 4:5:6:7 = 1, 5/4, 3/2, 7/4. Partch's otonality is the harmonic series; utonality is its mirror [S25].
- Utonal (undertone) mirror: reciprocals of the harmonic series. 1/6 : 1/5 : 1/4 scaled to integers is 10:12:15 = 1, 6/5, 3/2 (just minor triad). For a 7-limit utonal tetrad 1/7 : 1/6 : 1/5 : 1/4 the common multiple is 420, giving 60:70:84:105 = 1, 7/6, 7/5, 7/4. [DERIVED]
- Overtone drone stack (Dream-House style): choose a fundamental F and use harmonics `n*F` for chosen n (e.g. 1, 2, 3, 4, 5, 6, 7, 9...). Octave-reduce to taste: `r = n / 2^floor(log2 n)`.
- Choir triad (Section 4.4): 1, 5/4, 3/2 (major, 5-limit) or 1, 7/6, 3/2 (septimal minor) or 1, 9/7, 3/2.

Comma drift [DERIVED]: stacking four pure fifths (3/2)^4 and reducing by two octaves gives 81/16 / 4 = 81/64, which exceeds the pure major third 5/4 by exactly 81/80 = 21.51 cents [S32 gives the cents of 81/80]. Consequence for auto-tuning: if each voice tunes to a pure interval against *its neighbour* rather than the *root*, intervals accumulate drift (comma pumping). Always compute voice targets as ratios against a single fixed root (or an explicit lattice position), not by chaining neighbour-to-neighbour, unless you want drift as a feature.

### 2.4 Representing ratios (Euler lattice style) [DERIVED]

Store each target as a prime exponent vector over primes {2,3,5,7,11} (small ints). Ratio = `prod(p_i^e_i)`; cents = `sum(e_i * 1200*log2(p_i))`. This gives:
- exact arithmetic with no float drift (cents summed from constants),
- natural lattice moves (+1 on the 3-axis = up a fifth, +1 on the 5-axis = up a pure major third, -1 on the 7-axis = down a harmonic-seventh ratio),
- an octave normaliser (adjust the 2-exponent so cents lies in [0, 1200)).

This matches the genus/lattice idea of products of primes [S30] and Young's 3/2-by-7/4 lattice [S27].

### 2.5 Snap-to-nearest-just-interval (autotune of free voices) [DERIVED]

Given a voice's current frequency f, a root f0 and a sorted candidate set `T` of octave-reduced cents values (plus `T[0] + 1200` to handle wraparound):

```cpp
double snapCents(double f, double f0, const std::vector<double>& T, double strength)
{
    double c   = 1200.0 * std::log2(f / f0);
    double oct = std::floor(c / 1200.0);
    double r   = c - 1200.0 * oct;                 // 0..1200
    double best = T[0], bestD = std::abs(r - T[0]);
    for (double t : T) { double d = std::abs(r - t); if (d < bestD) { bestD = d; best = t; } }
    // also test last + wraparound: (T.front()+1200) and (T.back()-1200)
    double target = best + 1200.0 * oct;
    return c + strength * (target - c);            // strength 0..1 = how hard the snap pulls
}
```

Practical notes:
- Add hysteresis (only switch target when the new nearest beats the current by a few cents) to avoid chattering at midpoints.
- Glide with a one-pole in cents (`c_smooth += k*(c_target - c_smooth)`), time constants of 0.1 to 10 s for drone use.
- Because oscillators here are synthesised, "snap" is just a frequency target; no pitch detection is required. For free (user-dragged) pitch knobs, run the snap on the knob value. Pitch detection is only needed if later feeding external audio.
- Optional scoring across candidate scales or roots with a roughness measure (2.6).

### 2.6 Roughness / dissonance model (offline or control-rate)

Plomp-Levelt curve as parametrised by Sethares (as reproduced on a third-party page that cites it) [S33]:

```
d(f1, f2) = exp(-3.5 * s * (f2 - f1)) - exp(-5.75 * s * (f2 - f1)),   f1 < f2
s = 0.24 / (0.021 * f1 + 19)                                           (f1 = lower frequency)
D_total = sum over all pairs of partials
```

- Constants 0.24, 0.021, 19 verified at S33. Other sources quote 0.0207 and 18.96 [UNVERIFIED here]; the difference is negligible.
- S33 assumes equal partial intensity. Weighting by partial amplitude (e.g. `min(a1,a2)` or `a1*a2`) is in the literature but I could not verify which form Sethares prints in the book [UNVERIFIED].
- Cost: P partials per voice, 6 voices means (6P)^2/2 pair evaluations, each two `exp`. With P=16 that is about 4600 pairs, trivial at control rate (a few times per second), too heavy per sample.
- Use: sweep the interval ratio from 1 to 2, find local minima; those are timbre-matched "snap targets" instead of fixed JI ratios [S23]. For a pure sine drone the minima approach 1/1 only, so use harmonically rich waves for dissonance-curve tuning.

---

## 3. Dynamizing drones

| Technique | What it does | Real-time cost | Source / basis |
|---|---|---|---|
| Slow LFOs at incommensurate rates (e.g. 0.037, 0.053, 0.071 Hz) | Per-voice parameter drift that never exactly repeats on musical timescales | Very cheap (one sine/tri per voice per param) | [DERIVED]; same principle as Eno's loops below |
| Coprime / irrational rate ratios | Combined pattern period is the LCM of the periods; with coprime integer ratios it is `T1*T2` and with irrational ratios it never repeats | Free | [DERIVED] |
| Eno-style incommensurate loops | Loops of different lengths phase against each other; Discreet Music used 63 s and 68 s loops; Music for Airports "2/1" uses seven loops of different lengths [S34] | Cheap if replaced by LFOs/envelopes with different periods; heavy if real audio loops | [S34] |
| Smoothed random (S&H plus slew) | Stepped random with glide | Very cheap | Slew/S&H described in synth docs [S35, S36] |
| Random-segment ramp LFO | Ramp to new random levels over random segment lengths; the ramp acts as a frequency multiplier for a sine, plus zero-crossing-triggered amplitude randomisation | Cheap | musicdsp smooth random LFO [S37] |
| Perlin / value noise | Continuous, smooth, non-repeating wobble; Vital reportedly uses it [S20 forum comment]; Rust library docs describe 1D Perlin vs value noise [S36] | Cheap (interpolated lattice lookup) | [S20, S36] |
| Detuned unison / ensemble | Beating between near-unison voices; use non-linear detune law [S17] | Cost scales with voice count | [S7, S17] |
| Slow filter / wavetable / PWM modulation | Spectrum moves slowly without pitch change | Cheap (per-block param update, smoothed) | [DERIVED] |
| Spectral morphing (crossfade tables or PADsynth profiles) | Slow timbre change; PADsynth tables are precomputed so the runtime is just table playback and crossfade | Cheap at runtime, heavy at table build (do on a worker thread) | [S12, S16, S18] |
| Filter feedback | Resonant self-feedback adds moving harmonics | Moderate; needs saturation to stay stable | [S3] |
| Reverb / shimmer | Smears and thickens; shimmer adds octave-shifted tail | Moderate to heavy | sibling project Millenia (Section 4.6) |
| Granular tricks | Freeze/stretch of a captured buffer | Moderate; needs a buffer and a scheduler | [DERIVED; no source consulted] |

Design takeaways [DERIVED]:
- Randomness should be bounded and smoothed. Unsmoothed S&H on pitch or gain produces clicks and zipper noise; always slew (one-pole, 0.5 to 20 s).
- Modulate in perceptual units: cents for pitch, dB for gain, octaves for cutoff.
- Use a seeded PRNG (xorshift) so a preset plus seed reproduces behaviour, and expose "reseed" separately. Avoid `std::rand`/allocation on the audio thread.

---

## 4. Implications for Ethernauta

Difficulty: S = small (hours), M = medium (days), L = large (week+). Priority 1 is first.

### 4.1 Per-voice parameters (priority 1, S)

Per voice: `enabled/latch`, `gain (dB)`, `waveform` (sine, triangle, saw, square/PWM, plus a PADsynth-style choir/pad table later), `pitch` (root-relative: octave, ratio index or lattice coords, plus fine cents), `detune/drift amount`, `pan`, `filter cutoff` (optional), `mod depth` per source. Keep the **target** parameters and the **smoothed** realised values separate; smooth in the right domain (cents, dB). Use JUCE `AudioProcessorValueTreeState` with `SmoothedValue` for gain and a one-pole in cents for pitch. Latch = hold the voice sounding until toggled off; fade in/out with a 20 to 200 ms gain ramp to avoid clicks. [DERIVED]

### 4.2 Tuning engine (priority 2, M)

Data structures [DERIVED]:

```cpp
struct Ratio { int8_t e[5]; /* exponents of 2,3,5,7,11 */ };      // exact JI identity
struct Target { Ratio r; double cents; };                        // cents cached, octave-reduced
struct TuningTable { std::vector<Target> targets; };             // sorted by cents
struct TuningState {
    double rootHz;                    // global root
    TuningTable table;                // scale / chord set (JI, 12-TET, Partch diamond, ...)
    double snapStrength, glideSec, hysteresisCents;
};
```

- Presets: harmonic series (n = 1..16), 5-limit diamond (7 ratios) and 7-limit diamond (13) from [S26], Young-style 3 and 7 lattice [S27], 12-TET for comparison, and Scala-style custom lists later.
- Pipeline per voice: user pitch -> optional snap (2.5) -> glide in cents -> `exp2` -> oscillator increment.
- Targets always referenced to the one root, never chained (comma drift, 2.3).
- Offer a "pure vs tempered" macro that crossfades the snap target between JI and 12-TET (cents difference table in 2.3 shows how large each shift is).
- Optional second stage (priority 4): roughness-driven adaptive target choice using 2.6, computed on a worker/low-rate timer, never in the audio callback.

### 4.3 Modulation engine (priority 3, M)

- Sources: sine LFO, triangle, smooth-random (random-segment ramp, S37), value/Perlin noise, S&H+slew. All cheap [S35, S36, S37].
- Assignment: each source has a rate range and a seeded phase; per-voice sources get prime-ish rates (e.g. rate = base * {1, 1.31, 1.73, 2.09, ...}) so voices never lock [DERIVED].
- Destinations: pitch (cents, small, 0 to 10 cents), gain (dB), pan, filter cutoff, PWM width, wavetable position. Depth and rate global macros plus per-voice multipliers.
- Evaluate at control rate (every 32 to 64 samples) and interpolate; only audio-rate oscillators run per sample. Use a pre-allocated modulation matrix (fixed 6 voices x N sources x M destinations) to keep it real-time safe.
- Keep modulation depth conservative on pitch when voices are tuned: more than a few cents audibly un-snaps a just chord.

### 4.4 Three-voice choir group (priority 5, M to L)

Concept: three voices act as one "choir" member set (bass/tenor/alto-like register, or root/third/fifth), sharing a vowel/formant state.
- Pitch: stack from a group root using a ratio triad (1, 5/4, 3/2, or 7-limit variants, Section 2.3). Each member gets a small independent slow cent drift (1 to 6 cents) for ensemble feel. [DERIVED]
- Timbre: harmonically rich source (saw or PADsynth-style table) through a formant filter bank (parallel band-passes). Formant data: Csound vowel tables (5 vowels, 5 voice types, first 5 formants with frequency, amplitude, bandwidth) [S38]; an example soprano "a" is 800, 1150, 2900, 3900, 4950 Hz, amplitudes 0, -6, -32, -20, -50 dB, bandwidths 80, 90, 100, 130, 135 Hz, but sources disagree on some values, so re-check against the Csound manual before shipping [S21, partially UNVERIFIED].
- Fixed formants vs pitch: because a drone's pitch is static, formants can be fixed per vowel (this is what makes it sound like a held vowel). Real sopranos move F1 with pitch above about the first formant; that effect is not needed for a drone [claim not verified; I could not extract the SATB paper text, S39].
- Vowel morph: interpolate formant frequencies in log domain (or in cents) over seconds, driven by a slow smooth-random source; interpolate bandwidths linearly. [DERIVED]
- Alternative or complement: PADsynth with wide higher-harmonic bandwidth produces a natural choir/ensemble character with no formant filters; sources say wider bands for higher harmonics match natural choirs and ensembles [S12, S18]. Build the table on a background thread, crossfade on swap.
- Difficulty: M with PADsynth only; L with live formant filtering plus vowel morph.

### 4.5 Voice linking (priority 4, M)

- A link = a relation `child.pitch = parent.pitch * Ratio` (exact exponent vector), optionally with `child.gain = parent.gain + offset dB`, and shared modulation (child follows parent's LFO phase offset).
- Data: `struct Link { int parent; Ratio ratio; bool followGain; float modShare; }` per voice, with the constraint that the link graph is a forest (reject cycles at edit time). Evaluate in topological order each block.
- Linking to a **root** (the group root) rather than to another voice avoids comma drift (2.3).
- UI: "link to voice N, interval: unison / 3:2 / 5:4 / 7:4 / octave / custom ratio".

### 4.6 Effects stage (priority 6, optional)

Sibling project Millenia (Dattorro-based shimmer reverb) could later serve as a post stage for tail and octave-shift shimmer; keep Ethernauta's output bus plain stereo so that integration needs no engine changes. [DERIVED]

### 4.7 Suggested roadmap

1. Voices: oscillators, latch, gain, smooth parameters (S).
2. Tuning engine with ratio table, root, glide (M).
3. Modulation engine: incommensurate LFOs, smooth-random, noise (M).
4. Voice linking (M).
5. Choir group: PADsynth table first, formants second (M to L).
6. Roughness-aware snapping (offline/low-rate) (M).
7. Millenia-style effects stage (S to M).

### 4.8 Open questions I could not resolve

- Exact Dream House frequency list and tuning ratios (only secondary summaries found; S28, S29).
- Which amplitude-weighting form Sethares prints for pairwise dissonance (S33 assumes equal intensity).
- Exact JP-8000 supersaw detune curve coefficients (secondary summary only; S17).
- Pure Data's precise licence terms (README defers to LICENSE.txt; S11).
- JUCE version to target: JUCE 8 licence page [S14] vs `master` mentioning JUCE 9 EULA [S15].

---

## Sources

- S1 Vital: https://github.com/mtytel/vital
- S2 Surge: https://github.com/surge-synthesizer/surge
- S3 Surge XT manual: https://surge-synthesizer.github.io/manual-xt/
- S4 Cardinal: https://github.com/DISTRHO/Cardinal
- S5 ZynAddSubFX: https://github.com/zynaddsubfx/zynaddsubfx
- S6 Odin 2: https://github.com/TheWaveWarden/odin2 ; licence file https://raw.githubusercontent.com/TheWaveWarden/odin2/master/LICENSE
- S7 Helm: https://github.com/mtytel/helm
- S8 Dexed: https://github.com/asb2m10/dexed
- S9 SuperCollider: https://github.com/supercollider/supercollider
- S10 Csound: https://github.com/csound/csound
- S11 Pure Data: https://github.com/pure-data/pure-data
- S12 PADsynth algorithm (ZynAddSubFX docs): https://zynaddsubfx.sourceforge.io/doc/PADsynth/PADsynth.htm
- S13 JUCE README (licensing section): https://github.com/juce-framework/JUCE
- S14 JUCE 8 licence page: https://juce.com/legal/juce-8-licence/
- S15 JUCE LICENSE.md: https://raw.githubusercontent.com/juce-framework/JUCE/master/LICENSE.md
- S16 PADsynth wikibook: https://en.wikibooks.org/wiki/ZynAddSubFX/PADsynth/Algorithm
- S17 Supersaw analysis (Szabo thesis, via secondary search results): https://engineered.at/articles/emulating-a-jp-8000-supersaw-in-supercollider ; https://schollz.com/til/221103
- S18 Nasca, PADsynth paper: https://www.cs.ubbcluj.ro/~studia-i/contents/2011-1/04-Nasca.pdf
- S19 Csound GENpadsynth: https://csound.com/manual/GENpadsynth.html
- S20 Sonic Pi forum on Perlin modulation and Vital: https://in-thread.sonic-pi.net/t/perlin-modulation/4627
- S21 Haskell port of Csound formant tables: https://hackage.haskell.org/package/hsc3-lang-0.11/docs/Sound-SC3-Lang-Data-Vowel.html
- S22 Plomp and Levelt 1965: https://www.mpi.nl/publications/item66382/tonal-consonance-and-critical-bandwidth ; PDF https://www.math.miami.edu/~armstrong/592sp15/Plomp_Levelt_1965.pdf
- S23 Sethares, local consonance article: https://sethares.engr.wisc.edu/consemi.html
- S24 Sethares book (third-party PDF, not read): http://www.r-5.org/files/books/rx-music/tuning/William_A_Sethares-Tuning_Timbre_Spectrum_Scale-EN.pdf
- S25 Genesis of a Music (Partch): https://en.wikipedia.org/wiki/Genesis_of_a_Music
- S26 Tonality diamond: https://en.wikipedia.org/wiki/Tonality_diamond
- S27 The Well-Tuned Piano: https://en.wikipedia.org/wiki/The_Well-Tuned_Piano
- S28 Dream House installation: https://en.wikipedia.org/wiki/Dream_House_(installation)
- S29 Kyle Gann / Dream House material: https://melafoundation.org/gann.htm ; https://newmusicusa.org/nmbx/la-monte-young-and-marian-zazeela-at-the-dream-house/25/
- S30 Euler-Fokker genus: https://en.wikipedia.org/wiki/Euler%E2%80%93Fokker_genus
- S31 Playing Music in Just Intonation (adaptive tuning): https://arxiv.org/pdf/1706.04338
- S32 Cent: https://en.wikipedia.org/wiki/Cent_(music)
- S33 Consonance calculations (Sethares parametrisation, code): https://alpof.wordpress.com/2015/04/05/consonance-calculations-1/
- S34 Eno / Music for Airports: https://en.wikipedia.org/wiki/Ambient_1:_Music_for_Airports ; https://reverbmachine.com/blog/deconstructing-brian-eno-music-for-airports/
- S35 Noise LFO (S&H plus slew): https://gforcesoftware.notion.site/Noise-LFO-25540140f5408076aeaef3893a8c5d1b ; https://www.perfectcircuit.com/signal/learning-synthesis-random-1
- S36 Perlin vs value noise helpers: https://docs.rs/rust-synth/0.23.0/rust_synth/math/rnd/
- S37 musicdsp smooth random LFO: https://www.musicdsp.org/en/latest/Synthesis/269-smooth-random-lfo-generator.html
- S38 Csound vowelsdb opcode: https://csound-plugins.github.io/csound-plugins/opcodes/vowelsdb.html
- S39 SATB formants paper (text not extracted, not relied on): https://www.phys.unsw.edu.au/jw/reprints/SATB.pdf
