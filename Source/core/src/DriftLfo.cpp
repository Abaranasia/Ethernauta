#include <ethernauta/DriftLfo.h>

#include <cmath>

namespace ethernauta {
namespace {
constexpr double kTwoPi = 6.283185307179586476925287;
constexpr int kNumDistinctVoices = 8; // voice indices beyond this reuse the rates of index % 8

// Rates in Hz per voice index k: A = 0.050 + 0.012 k (0.050 .. 0.134), B = 0.083 + 0.0155 k
// (0.083 .. 0.1915). Neither pair is an integer ratio, so a voice's two components never lock, and
// no two voices share a rate.
constexpr double kRateABase = 0.050;
constexpr double kRateAStep = 0.012;
constexpr double kRateBBase = 0.083;
constexpr double kRateBStep = 0.0155;

// Start phases from the golden-ratio and plastic-number sequences: well spread, fixed, no random state.
inline double fraction(double x) noexcept { return x - std::floor(x); }
} // namespace

void DriftLfo::setVoiceIndex(int voiceIndex) noexcept
{
    const int k = voiceIndex < 0 ? 0 : voiceIndex % kNumDistinctVoices;
    const double dk = static_cast<double>(k);
    rateAHz_ = kRateABase + kRateAStep * dk;
    rateBHz_ = kRateBBase + kRateBStep * dk;
    phaseA_ = kTwoPi * fraction(0.6180339887498949 * (dk + 1.0));
    phaseB_ = kTwoPi * fraction(0.7548776662466927 * (dk + 1.0) + 0.3);
    updateSteps();
}

void DriftLfo::prepare(double sampleRate) noexcept
{
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 1.0;
    updateSteps();
}

void DriftLfo::updateSteps() noexcept
{
    stepA_ = kTwoPi * rateAHz_ / sampleRate_;
    stepB_ = kTwoPi * rateBHz_ / sampleRate_;
}

double DriftLfo::at(std::uint64_t sample) const noexcept
{
    const double n = static_cast<double>(sample);
    return 0.5 * (std::sin(phaseA_ + stepA_ * n) + std::sin(phaseB_ + stepB_ * n));
}

} // namespace ethernauta
