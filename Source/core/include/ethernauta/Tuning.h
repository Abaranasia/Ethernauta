#pragma once
#include <ethernauta/VoiceParams.h>

#include <cmath>

namespace ethernauta {

// Tuning math. All functions are pure, allocation-free and noexcept.
inline double ratioToCents(double ratio) noexcept { return 1200.0 * std::log2(ratio); }
inline double centsToRatio(double cents) noexcept { return std::exp2(cents / 1200.0); }
inline double ratioToSemitones(double ratio) noexcept { return 12.0 * std::log2(ratio); }
inline double semitonesToRatio(double semitones) noexcept { return std::exp2(semitones / 12.0); }

// Equal-tempered frequency of a (fractional) MIDI note, A4 = 69 = 440 Hz. Same convention as
// pitchToHz(), without its range clamping.
inline double midiNoteToHz(double note) noexcept { return 440.0 * std::exp2((note - 69.0) / 12.0); }

// Octave folding: result in [0, 1200) cents / [1, 2) ratio.
inline double foldCents(double cents) noexcept
{
    const double f = cents - 1200.0 * std::floor(cents / 1200.0);
    return f >= 1200.0 ? 0.0 : f; // a tiny negative input rounds up to exactly 1200
}
inline double foldRatio(double ratio) noexcept
{
    const double f = ratio / std::exp2(std::floor(std::log2(ratio)));
    return f >= 2.0 ? 1.0 : f;
}

// One just interval as plain data. `cents` is cached (9 decimals) and verified against num/den in tests;
// frequencies are always computed from the exact num/den.
struct JustInterval {
    int num;
    int den;
    double cents;
    const char* name;
    constexpr double ratio() const noexcept { return static_cast<double>(num) / static_cast<double>(den); }
};

// Layout of kJustTable. The order is a stable contract: a degree index is stored in presets and
// automation, so entries may only be appended.
//   [0, 15]   octave-reduced 5-/7-limit scale, sorted ascending by cents (the snap candidates)
//   [16]      the octave 2/1 (selectable degree; not a snap candidate, 1/1 of the next octave is)
//   [17, 32]  harmonic series partials 1..16
inline constexpr int kNumScaleDegrees = 16;
inline constexpr int kPartialBase = 17;
inline constexpr int kNumJustDegrees = 33;

inline constexpr JustInterval kJustTable[kNumJustDegrees] = {
    {1, 1, 0.000000000, "Unison"},
    {16, 15, 111.731285270, "Minor second"},
    {10, 9, 182.403712134, "Minor whole tone"},
    {9, 8, 203.910001731, "Major second"},
    {7, 6, 266.870905604, "Septimal minor third"},
    {6, 5, 315.641287001, "Minor third"},
    {5, 4, 386.313713865, "Major third"},
    {9, 7, 435.084095262, "Septimal major third"},
    {4, 3, 498.044999135, "Fourth"},
    {7, 5, 582.512192604, "Septimal tritone"},
    {3, 2, 701.955000865, "Fifth"},
    {8, 5, 813.686286135, "Minor sixth"},
    {5, 3, 884.358712999, "Major sixth"},
    {7, 4, 968.825906469, "Harmonic seventh"},
    {9, 5, 1017.596287866, "Minor seventh"},
    {15, 8, 1088.268714730, "Major seventh"},
    {2, 1, 1200.000000000, "Octave"},
    {1, 1, 0.000000000, "Partial 1"},
    {2, 1, 1200.000000000, "Partial 2"},
    {3, 1, 1901.955000865, "Partial 3"},
    {4, 1, 2400.000000000, "Partial 4"},
    {5, 1, 2786.313713865, "Partial 5"},
    {6, 1, 3101.955000865, "Partial 6"},
    {7, 1, 3368.825906469, "Partial 7"},
    {8, 1, 3600.000000000, "Partial 8"},
    {9, 1, 3803.910001731, "Partial 9"},
    {10, 1, 3986.313713865, "Partial 10"},
    {11, 1, 4151.317942365, "Partial 11"},
    {12, 1, 4301.955000865, "Partial 12"},
    {13, 1, 4440.527661769, "Partial 13"},
    {14, 1, 4568.825906469, "Partial 14"},
    {15, 1, 4688.268714730, "Partial 15"},
    {16, 1, 4800.000000000, "Partial 16"},
};

} // namespace ethernauta
