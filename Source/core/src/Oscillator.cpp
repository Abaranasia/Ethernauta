#include <ethernauta/Oscillator.h>

#include <cmath>

namespace ethernauta {
namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

// Polynomial band-limited step residual. t: phase in [0, 1), dt: phase increment per sample.
inline double polyBlep(double t, double dt) noexcept
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

inline double renderWave(Waveform w, double t, double dt) noexcept
{
    switch (w) {
        case Waveform::Sine:
            return std::sin(kTwoPi * t);
        case Waveform::Saw:
            return 2.0 * t - 1.0 - polyBlep(t, dt);
        case Waveform::Square: {
            double t2 = t + 0.5;
            if (t2 >= 1.0) t2 -= 1.0;
            return (t < 0.5 ? 1.0 : -1.0) + polyBlep(t, dt) - polyBlep(t2, dt);
        }
        case Waveform::Triangle:
            return 4.0 * std::fabs(t - 0.5) - 1.0;
    }
    return 0.0;
}

} // namespace

void Oscillator::prepare(double sampleRate) noexcept
{
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 1.0;
    phase_ = 0.0;
    fadeStep_ = 1.0 / (kCrossfadeSeconds * sampleRate_ > 1.0 ? kCrossfadeSeconds * sampleRate_ : 1.0);
    fading_ = false;
    hasPending_ = false;
    setFrequency(frequency_);
}

void Oscillator::startFade(Waveform to) noexcept
{
    from_ = waveform_;
    waveform_ = to;
    fade_ = 0.0;
    fading_ = true;
}

void Oscillator::crossfadeTo(Waveform w) noexcept
{
    if (fading_) {
        if (w == waveform_) {
            hasPending_ = false; // asked to stay on the current target
        } else {
            pending_ = w;
            hasPending_ = true;
        }
    } else if (w != waveform_) {
        startFade(w);
    }
}

void Oscillator::setFrequency(double hz) noexcept
{
    if (!(hz > 0.0)) hz = 0.0; // also catches NaN
    const double maxHz = 0.49 * sampleRate_;
    if (hz > maxHz) hz = maxHz;
    frequency_ = hz;
    increment_ = hz / sampleRate_;
}

float Oscillator::process() noexcept
{
    const double t = phase_;
    const double dt = increment_;
    double v = renderWave(waveform_, t, dt);
    if (fading_) {
        const double old = renderWave(from_, t, dt);
        v = old + (v - old) * fade_;
        fade_ += fadeStep_;
        if (fade_ >= 1.0) {
            fading_ = false;
            if (hasPending_) {
                hasPending_ = false;
                startFade(pending_);
            }
        }
    }
    phase_ += dt;
    if (phase_ >= 1.0) phase_ -= 1.0;
    return static_cast<float>(v);
}

} // namespace ethernauta
