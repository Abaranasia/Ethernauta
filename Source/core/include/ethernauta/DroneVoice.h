#pragma once
#include <ethernauta/DriftLfo.h>
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
// Contract: an idle voice always sits at its target (a glide in flight when the gate goes idle
// completes instantly), so a relatch starts there. setFrequencyHz() overrides params_.midiNote/cents,
// which only take effect through setPitch()/setParams(): the two entry points to the frequency.
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

    // Choir drift: a slow, deterministic pitch offset of up to +-depth cents (see DriftLfo) added in the
    // cents domain on top of the target and the glide, per sample, so it never changes the tuned
    // target. depth is clamped to [0, kMaxDriftCents] (non-finite values are ignored); 0, the default,
    // renders bit-identically to a voice without drift. The depth is smoothed (no click when it
    // changes), except while idle where it takes effect at once. The drift runs only while the voice
    // sounds: an idle voice has no offset (it sits at its target, as above) and the drift clock
    // advances per rendered sample, so a relatch continues where it stopped. Voices with different
    // indices drift differently; prepare() and reset() restart the clock.
    void setDriftDepth(float cents) noexcept;
    float driftDepth() const noexcept { return driftDepth_.target(); }
    void setDriftVoiceIndex(int voiceIndex) noexcept { lfo_.setVoiceIndex(voiceIndex); }

    double frequencyHz() const noexcept;       // current (gliding, drifting) frequency
    double targetFrequencyHz() const noexcept { return targetHz_; }

    static constexpr double kGlideSeconds = 0.08; // time constant of the retune glide

    // True while the voice can produce non-zero output.
    bool isActive() const noexcept { return !gate_.isIdle(); }

    void process(float* out, int numSamples) noexcept;

private:
    static constexpr double kAttackSeconds = 0.02;
    static constexpr double kReleaseSeconds = 0.3;
    static constexpr double kGainSmoothingSeconds = 0.02;
    static constexpr double kDriftDepthSmoothingSeconds = 0.25;

    void jumpToFrequency(double hz) noexcept;
    void completeGlide() noexcept; // snap an in-flight glide to its target

    VoiceParams params_{};
    double targetHz_ = 110.0;
    double glideCents_ = 0.0;       // current frequency as 1200 * log2(Hz)
    double glideTargetCents_ = 0.0;
    double glideCoeff_ = 1.0;
    bool gliding_ = false;
    DriftLfo lfo_;
    ParamSmoother driftDepth_;       // smoothed depth in cents
    std::uint64_t driftSample_ = 0;  // rendered (sounding) samples since prepare()/reset()
    double driftCents_ = 0.0;        // offset currently applied to the oscillator
    Oscillator osc_;
    LatchGate gate_;
    ParamSmoother gain_;
};

} // namespace ethernauta
