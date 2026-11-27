#pragma once
#include <ethernauta/Waveform.h>

namespace ethernauta {

// Band-limited oscillator: sine (exact), saw and square (PolyBLEP), triangle (naive).
//
// The triangle has no amplitude discontinuity, only a slope break, so it is left uncorrected; its
// aliasing falls off at 12 dB/octave. A PolyBLAMP correction can be added later without API changes.
// Output range is about [-1, 1] (PolyBLEP overshoot stays below ~1.1). Phase is a double accumulator.
// process() is allocation-free, lock-free and exception-free.
//
// Waveform changes: setWaveform() switches immediately (use it while silent); crossfadeTo() blends
// from the current to the new waveform over kCrossfadeSeconds, both read from the same phase, so a
// switch while sounding has no discontinuity. A request made during a fade is queued (latest wins)
// and starts when the running fade ends, which keeps the output continuous.
class Oscillator {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept { phase_ = 0.0; }

    void setWaveform(Waveform w) noexcept
    {
        waveform_ = w;
        fading_ = false;
        hasPending_ = false;
    }
    void crossfadeTo(Waveform w) noexcept;
    // Hz; clamped to [0, 0.49 * sampleRate] so the phase increment is always valid.
    void setFrequency(double hz) noexcept;

    // The waveform being faded to (or already playing).
    Waveform waveform() const noexcept { return waveform_; }
    double frequency() const noexcept { return frequency_; }

    float process() noexcept;

private:
    static constexpr double kCrossfadeSeconds = 0.005;

    void startFade(Waveform to) noexcept;

    double sampleRate_ = 48000.0;
    double frequency_ = 110.0;
    double phase_ = 0.0; // [0, 1)
    double increment_ = 110.0 / 48000.0;
    Waveform waveform_ = Waveform::Sine;
    Waveform from_ = Waveform::Sine;    // waveform being faded out
    Waveform pending_ = Waveform::Sine; // queued target, valid when hasPending_
    double fade_ = 0.0;                 // 0 = all from_, 1 = all waveform_
    double fadeStep_ = 1.0 / (kCrossfadeSeconds * 48000.0);
    bool fading_ = false;
    bool hasPending_ = false;
};

} // namespace ethernauta
