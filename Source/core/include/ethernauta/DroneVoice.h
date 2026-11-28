#pragma once
#include <ethernauta/LatchGate.h>
#include <ethernauta/Oscillator.h>
#include <ethernauta/ParamSmoother.h>
#include <ethernauta/VoiceParams.h>

namespace ethernauta {

// One drone voice: Oscillator -> latch gate -> smoothed gain.
//
// Frequency: setFrequencyHz() (and setPitch(), which converts to Hz) sets a target. While the voice
// is idle the frequency jumps there at once; while it sounds, the frequency glides to the target in
// the log (cents) domain with a one-pole of kGlideSeconds, so retuning (root change, tuning mode
// change, pitch knob) never clicks. The glide advances per sample, so block size does not matter.
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
    // Target frequency in Hz (clamped to a safe range; non-finite values are ignored). Cheap to call
    // every block with an unchanged value.
    void setFrequencyHz(double hz) noexcept;

    double frequencyHz() const noexcept;       // current (gliding) frequency
    double targetFrequencyHz() const noexcept { return targetHz_; }

    static constexpr double kGlideSeconds = 0.08; // time constant of the retune glide

    // True while the voice can produce non-zero output.
    bool isActive() const noexcept { return !gate_.isIdle(); }

    void process(float* out, int numSamples) noexcept;

private:
    static constexpr double kAttackSeconds = 0.02;
    static constexpr double kReleaseSeconds = 0.3;
    static constexpr double kGainSmoothingSeconds = 0.02;

    void jumpToFrequency(double hz) noexcept;

    VoiceParams params_{};
    double targetHz_ = 110.0;
    double glideCents_ = 0.0;       // current frequency as 1200 * log2(Hz)
    double glideTargetCents_ = 0.0;
    double glideCoeff_ = 1.0;
    bool gliding_ = false;
    Oscillator osc_;
    LatchGate gate_;
    ParamSmoother gain_;
};

} // namespace ethernauta
