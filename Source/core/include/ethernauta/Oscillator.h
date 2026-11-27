#pragma once
#include <ethernauta/Waveform.h>

namespace ethernauta {

// Band-limited oscillator: sine (exact), saw and square (PolyBLEP), triangle (naive).
//
// The triangle has no amplitude discontinuity, only a slope break, so it is left uncorrected; its
// aliasing falls off at 12 dB/octave. A PolyBLAMP correction can be added later without API changes.
// Output range is about [-1, 1] (PolyBLEP overshoot stays below ~1.1). Phase is a double accumulator.
// process() is allocation-free, lock-free and exception-free.
class Oscillator {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept { phase_ = 0.0; }

    void setWaveform(Waveform w) noexcept { waveform_ = w; }
    // Hz; clamped to [0, 0.49 * sampleRate] so the phase increment is always valid.
    void setFrequency(double hz) noexcept;

    Waveform waveform() const noexcept { return waveform_; }
    double frequency() const noexcept { return frequency_; }

    float process() noexcept;

private:
    double sampleRate_ = 48000.0;
    double frequency_ = 110.0;
    double phase_ = 0.0; // [0, 1)
    double increment_ = 110.0 / 48000.0;
    Waveform waveform_ = Waveform::Sine;
};

} // namespace ethernauta
