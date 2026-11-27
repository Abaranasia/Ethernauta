#include <ethernauta/LatchGate.h>

namespace ethernauta {

void LatchGate::prepare(double sampleRate, double attackSeconds, double releaseSeconds) noexcept
{
    const double a = attackSeconds * sampleRate;
    const double r = releaseSeconds * sampleRate;
    attackStep_ = a > 1.0 ? static_cast<float>(1.0 / a) : 1.0f;
    releaseStep_ = r > 1.0 ? static_cast<float>(1.0 / r) : 1.0f;
    progress_ = 0.0f;
}

float LatchGate::next() noexcept
{
    if (open_) {
        progress_ += attackStep_;
        if (progress_ > 1.0f) progress_ = 1.0f;
    } else if (progress_ > 0.0f) {
        progress_ -= releaseStep_;
        if (progress_ < 0.0f) progress_ = 0.0f;
    }
    const float p = progress_;
    return p * p * (3.0f - 2.0f * p); // smoothstep; exactly 0 at p=0 and exactly 1 at p=1
}

} // namespace ethernauta
