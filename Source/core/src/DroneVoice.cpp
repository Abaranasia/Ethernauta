#include <ethernauta/DroneVoice.h>

namespace ethernauta {

void DroneVoice::prepare(double sampleRate) noexcept
{
    osc_.prepare(sampleRate);
    gate_.prepare(sampleRate, kAttackSeconds, kReleaseSeconds);
    gain_.prepare(sampleRate, kGainSmoothingSeconds);
    const VoiceParams p = params_; // re-apply the stored settings at the new rate
    setParams(p);
    gain_.snapTo(gainDbToLinear(p.gainDb));
}

void DroneVoice::reset() noexcept
{
    osc_.reset();
    gate_.reset();
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
    // Opening from idle: the gain must not glide in from a stale value; the attack alone fades in.
    if (latched && gate_.isIdle()) gain_.snapTo(gain_.target());
    gate_.setOpen(latched);
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

void DroneVoice::setPitch(float midiNote, float cents) noexcept
{
    params_.midiNote = midiNote;
    params_.cents = cents;
    osc_.setFrequency(pitchToHz(midiNote, cents));
}

void DroneVoice::process(float* out, int numSamples) noexcept
{
    // Setters only run between blocks, so the idle state cannot change mid-block and this early
    // return is equivalent to rendering zeros (and keeps block-size independence).
    if (out == nullptr || numSamples <= 0 || gate_.isIdle()) return;
    for (int i = 0; i < numSamples; ++i) {
        const float env = gate_.next();
        const float g = gain_.next();
        out[i] += osc_.process() * env * g;
    }
}

} // namespace ethernauta
