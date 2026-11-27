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
    setLatched(p.latched);
    setGainDb(p.gainDb);
    setPitch(p.midiNote, p.cents);
}

void DroneVoice::setWaveform(Waveform w) noexcept
{
    params_.waveform = w;
    osc_.setWaveform(w);
}

void DroneVoice::setLatched(bool latched) noexcept
{
    params_.latched = latched;
    gate_.setOpen(latched);
}

void DroneVoice::setGainDb(float db) noexcept
{
    params_.gainDb = db;
    gain_.setTarget(gainDbToLinear(db));
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
