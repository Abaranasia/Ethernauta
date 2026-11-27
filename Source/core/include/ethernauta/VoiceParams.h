#pragma once
#include <ethernauta/Waveform.h>

#include <cmath>

namespace ethernauta {

// Gain is expressed in decibels. At or below kMinGainDb the voice is muted (linear gain exactly 0).
inline constexpr float kMinGainDb = -60.0f;
inline constexpr float kMaxGainDb = 12.0f;
// Pitch is a (fractional) MIDI note number plus a fine-tune in cents. A2 = note 45 = 110 Hz.
inline constexpr float kMinMidiNote = 0.0f;
inline constexpr float kMaxMidiNote = 127.0f;
inline constexpr float kMaxCents = 100.0f;

// Plain-data description of ONE drone voice: the single source of truth that the plugin layer fills
// from its parameters. Six voices are six VoiceParams (e.g. std::array<VoiceParams, 6>).
struct VoiceParams {
    Waveform waveform = Waveform::Sine;
    bool latched = false;    // true: keeps sounding until set to false
    float gainDb = -12.0f;   // [kMinGainDb, kMaxGainDb]
    float midiNote = 45.0f;  // A2 = 110 Hz
    float cents = 0.0f;      // [-kMaxCents, +kMaxCents]
};

// Equal-tempered frequency of a fractional MIDI note plus cents (A4 = note 69 = 440 Hz).
// Inputs are clamped to their documented ranges so the result is always finite and positive.
inline double pitchToHz(float midiNote, float cents) noexcept
{
    const double note = midiNote < kMinMidiNote ? kMinMidiNote : (midiNote > kMaxMidiNote ? kMaxMidiNote : midiNote);
    const double c = cents < -kMaxCents ? -kMaxCents : (cents > kMaxCents ? kMaxCents : cents);
    return 440.0 * std::exp2((note - 69.0 + c / 100.0) / 12.0);
}

// Decibels to linear amplitude; <= kMinGainDb maps to exactly 0, > kMaxGainDb is clamped.
inline float gainDbToLinear(float db) noexcept
{
    if (!(db > kMinGainDb)) return 0.0f; // also catches NaN
    if (db > kMaxGainDb) db = kMaxGainDb;
    return static_cast<float>(std::pow(10.0, static_cast<double>(db) / 20.0));
}

} // namespace ethernauta
