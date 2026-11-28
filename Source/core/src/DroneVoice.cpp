#include <ethernauta/DroneVoice.h>

#include <cmath>

namespace ethernauta {
namespace {
constexpr double kMinHz = 1.0;
constexpr double kMaxHz = 1.0e6; // the oscillator clamps to 0.49 * sampleRate on top of this
constexpr double kGlideSnapCents = 1.0e-6;
inline double hzToCents(double hz) noexcept { return 1200.0 * std::log2(hz); }
inline double centsToHz(double cents) noexcept { return std::exp2(cents / 1200.0); }
} // namespace

void DroneVoice::prepare(double sampleRate) noexcept
{
    osc_.prepare(sampleRate);
    gate_.prepare(sampleRate, kAttackSeconds, kReleaseSeconds);
    gain_.prepare(sampleRate, kGainSmoothingSeconds);
    lfo_.prepare(sampleRate);
    driftDepth_.prepare(sampleRate, kDriftDepthSmoothingSeconds);
    driftDepth_.snapTo(driftDepth_.target()); // no ramp from a stale value
    driftSample_ = 0;
    const double samples = kGlideSeconds * sampleRate;
    glideCoeff_ = samples > 1.0 ? 1.0 - std::exp(-1.0 / samples) : 1.0;
    const VoiceParams p = params_; // re-apply the stored settings at the new rate
    setWaveform(p.waveform);
    setGainDb(p.gainDb);
    jumpToFrequency(targetHz_); // keep a tuned frequency; do not revert to the pitch parameter
    setLatched(p.latched);
    gain_.snapTo(gainDbToLinear(p.gainDb));
}

void DroneVoice::reset() noexcept
{
    osc_.reset();
    gate_.reset();
    driftSample_ = 0;
    driftDepth_.snapTo(driftDepth_.target());
    if (gate_.isIdle()) completeGlide();
    gain_.snapTo(gainDbToLinear(params_.gainDb));
}

void DroneVoice::setParams(const VoiceParams& p) noexcept
{
    setWaveform(p.waveform);
    setGainDb(p.gainDb);
    setPitch(p.midiNote, p.cents);
    setLatched(p.latched); // last: waveform and gain are already final when the gate opens
}

void DroneVoice::setWaveform(Waveform w) noexcept
{
    params_.waveform = w;
    // Idle: nothing is audible, switch at once so the next latch starts on the new waveform.
    // Sounding (or releasing): crossfade to avoid a click.
    if (gate_.isIdle())
        osc_.setWaveform(w);
    else
        osc_.crossfadeTo(w);
}

void DroneVoice::setLatched(bool latched) noexcept
{
    params_.latched = latched;
    // Opening from idle: the gain must not glide in from a stale value; the attack alone fades in,
    // and a pending glide completes first so the voice never resumes from a stale pitch.
    if (gate_.isIdle()) {
        completeGlide();
        if (latched) gain_.snapTo(gain_.target());
    }
    gate_.setOpen(latched);
    if (gate_.isIdle()) completeGlide(); // closing before any envelope progress idles at once
}

void DroneVoice::setGainDb(float db) noexcept
{
    params_.gainDb = db;
    const float target = gainDbToLinear(db);
    // While idle nothing is audible, so jump to the target instead of ramping toward it later.
    if (gate_.isIdle())
        gain_.snapTo(target);
    else
        gain_.setTarget(target);
}

void DroneVoice::setDriftDepth(float cents) noexcept
{
    if (!std::isfinite(cents)) return;
    const float depth = cents < 0.0f ? 0.0f : (cents > static_cast<float>(kMaxDriftCents) ? static_cast<float>(kMaxDriftCents) : cents);
    // While idle nothing is audible, so take the depth at once instead of ramping on the next latch.
    if (gate_.isIdle())
        driftDepth_.snapTo(depth);
    else
        driftDepth_.setTarget(depth);
}

void DroneVoice::setPitch(float midiNote, float cents) noexcept
{
    params_.midiNote = midiNote;
    params_.cents = cents;
    setFrequencyHz(pitchToHz(midiNote, cents));
}

void DroneVoice::jumpToFrequency(double hz) noexcept
{
    targetHz_ = hz;
    glideCents_ = glideTargetCents_ = hzToCents(hz);
    gliding_ = false;
    driftCents_ = 0.0;
    osc_.setFrequency(hz);
}

void DroneVoice::completeGlide() noexcept
{
    if (gliding_ || driftCents_ != 0.0) jumpToFrequency(targetHz_); // also drops any drift offset
}

void DroneVoice::setFrequencyHz(double hz) noexcept
{
    if (!std::isfinite(hz)) return;
    hz = hz < kMinHz ? kMinHz : (hz > kMaxHz ? kMaxHz : hz);
    // Idle: nothing is audible, so jump instead of gliding from a stale value on the next latch.
    if (gate_.isIdle()) {
        jumpToFrequency(hz);
        return;
    }
    if (hz == targetHz_) return;
    targetHz_ = hz;
    glideTargetCents_ = hzToCents(hz);
    gliding_ = true;
}

double DroneVoice::frequencyHz() const noexcept
{
    return (gliding_ || driftCents_ != 0.0) ? centsToHz(glideCents_ + driftCents_) : targetHz_;
}

void DroneVoice::process(float* out, int numSamples) noexcept
{
    if (out == nullptr || numSamples <= 0) return;
    if (gate_.isIdle()) {
        completeGlide();
        return;
    }
    for (int i = 0; i < numSamples; ++i) {
        // Drift: runs only while the depth is (or is ramping to) non-zero, so depth 0 takes exactly the
        // pre-drift path below. The clock always counts rendered samples, so the value depends only on
        // how many samples have sounded, never on block boundaries.
        const bool driftRuns = driftDepth_.target() != 0.0f || !driftDepth_.isSettled();
        const double drift = driftRuns ? static_cast<double>(driftDepth_.next()) * lfo_.at(driftSample_) : 0.0;
        ++driftSample_;
        bool retune = false;
        if (gliding_) {
            const double diff = glideTargetCents_ - glideCents_;
            if (std::fabs(diff) < kGlideSnapCents) {
                glideCents_ = glideTargetCents_;
                gliding_ = false;
            } else {
                glideCents_ += diff * glideCoeff_;
            }
            retune = true;
        }
        if (driftRuns || driftCents_ != 0.0) {
            driftCents_ = drift;
            retune = true;
        }
        if (retune)
            osc_.setFrequency((gliding_ || driftCents_ != 0.0) ? centsToHz(glideCents_ + driftCents_) : targetHz_);
        const float env = gate_.next();
        // The gate just went idle: stop here so the oscillator and gain stop advancing at the same
        // sample for any block size, and settle the glide so a relatch starts at the target.
        if (gate_.isIdle()) {
            completeGlide();
            return;
        }
        const float g = gain_.next();
        out[i] += osc_.process() * env * g;
    }
}

} // namespace ethernauta
