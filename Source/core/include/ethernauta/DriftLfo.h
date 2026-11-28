#pragma once
#include <cstdint>

namespace ethernauta {

// Largest choir drift depth in cents (research: each member drifts "1 to 6 cents").
inline constexpr double kMaxDriftCents = 6.0;

// Slow, deterministic, bounded drift source for one voice: the average of two sines at incommensurate
// slow rates (about 0.05-0.2 Hz), so the value is always within [-1, 1] and never settles into a
// short repeating pattern. Rates and start phases are fixed functions of the voice index (no rand(),
// no clock), so every voice drifts differently and every run is identical.
//
// The value is a pure function of the sample count: at(n) never depends on how earlier samples were
// grouped into blocks. Header + src, JUCE-free, allocation-free; at() is real-time safe.
class DriftLfo {
public:
    // Selects the rates and start phases (voice 0 for any negative index). Cheap, no allocation.
    void setVoiceIndex(int voiceIndex) noexcept;
    // Sets the radians-per-sample steps for this sample rate. Call before at() and on a rate change.
    void prepare(double sampleRate) noexcept;

    // Drift value in [-1, 1] after `sample` samples (sample 0 is the first sample after prepare()).
    double at(std::uint64_t sample) const noexcept;

    double rateAHz() const noexcept { return rateAHz_; }
    double rateBHz() const noexcept { return rateBHz_; }
    double phaseA() const noexcept { return phaseA_; } // radians at sample 0
    double phaseB() const noexcept { return phaseB_; }

private:
    void updateSteps() noexcept;

    double sampleRate_ = 48000.0;
    double rateAHz_ = 0.05;
    double rateBHz_ = 0.083;
    double phaseA_ = 0.0;
    double phaseB_ = 0.0;
    double stepA_ = 0.0; // radians per sample, set by prepare()/setVoiceIndex()
    double stepB_ = 0.0;
};

} // namespace ethernauta
