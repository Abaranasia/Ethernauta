#pragma once
#include <ethernauta/LatchGate.h>
#include <ethernauta/Oscillator.h>
#include <ethernauta/ParamSmoother.h>
#include <ethernauta/VoiceParams.h>

namespace ethernauta {

// One drone voice: Oscillator -> latch gate -> smoothed gain.
//
// Threading: configure and process from the same (audio) thread. Setters are cheap and
// allocation-free, so they may be called every block with the latest parameter values.
//
// Output contract: process() ADDS the voice into `out` (out[i] += sample). The caller clears the
// buffer once and then calls process() on each voice, so six voices mix with a simple loop. An idle
// voice (unlatched and fully released) does not touch the buffer at all.
//
// Determinism: all state advances per sample, so rendering N samples in one block or in any
// sequence of smaller blocks gives bit-identical output, provided setters are called at the same
// sample positions.
class DroneVoice {
public:
    // Call before processing and whenever the sample rate changes. Resets the oscillator phase and
    // gate, and snaps the gain to its target (no fade of the level itself). Not meant for the
    // audio callback.
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setParams(const VoiceParams& p) noexcept;
    const VoiceParams& params() const noexcept { return params_; }

    void setWaveform(Waveform w) noexcept;
    void setLatched(bool latched) noexcept;
    void setGainDb(float db) noexcept;
    void setPitch(float midiNote, float cents = 0.0f) noexcept;

    // True while the voice can produce non-zero output.
    bool isActive() const noexcept { return !gate_.isIdle(); }

    void process(float* out, int numSamples) noexcept;

private:
    static constexpr double kAttackSeconds = 0.02;
    static constexpr double kReleaseSeconds = 0.3;
    static constexpr double kGainSmoothingSeconds = 0.02;

    VoiceParams params_{};
    Oscillator osc_;
    LatchGate gate_;
    ParamSmoother gain_;
};

} // namespace ethernauta
