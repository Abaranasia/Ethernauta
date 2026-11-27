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

} // namespace

void Oscillator::prepare(double sampleRate) noexcept
{
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 1.0;
    phase_ = 0.0;
    setFrequency(frequency_);
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
    double v = 0.0;
    switch (waveform_) {
        case Waveform::Sine:
            v = std::sin(kTwoPi * t);
            break;
        case Waveform::Saw:
            v = 2.0 * t - 1.0 - polyBlep(t, dt);
            break;
        case Waveform::Square: {
            double t2 = t + 0.5;
            if (t2 >= 1.0) t2 -= 1.0;
            v = (t < 0.5 ? 1.0 : -1.0) + polyBlep(t, dt) - polyBlep(t2, dt);
            break;
        }
        case Waveform::Triangle:
            v = 4.0 * std::fabs(t - 0.5) - 1.0;
            break;
    }
    phase_ += dt;
    if (phase_ >= 1.0) phase_ -= 1.0;
    return static_cast<float>(v);
}

} // namespace ethernauta
