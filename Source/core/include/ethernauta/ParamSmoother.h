#pragma once
#include <cmath>

namespace ethernauta {

// One-pole (exponential) parameter smoother. Header-only, allocation-free.
// After prepare(), next() moves the value toward the target by a fixed fraction per sample. Once the
// remaining distance is below a tiny epsilon the value snaps exactly to the target, which avoids
// denormals and lets callers test isSettled().
class ParamSmoother {
public:
    void prepare(double sampleRate, double timeConstantSeconds) noexcept
    {
        const double samples = timeConstantSeconds * sampleRate;
        coeff_ = samples > 1.0 ? 1.0 - std::exp(-1.0 / samples) : 1.0;
    }
    void setTarget(float t) noexcept { target_ = static_cast<double>(t); }
    void snapTo(float v) noexcept { value_ = target_ = static_cast<double>(v); }
    float target() const noexcept { return static_cast<float>(target_); }
    float current() const noexcept { return static_cast<float>(value_); }
    bool isSettled() const noexcept { return value_ == target_; }

    float next() noexcept
    {
        const double diff = target_ - value_;
        if (std::fabs(diff) < kSnapEpsilon) {
            value_ = target_;
        } else {
            value_ += diff * coeff_;
        }
        return static_cast<float>(value_);
    }

private:
    // State is double: a float state would stall short of the target once diff * coeff drops below
    // the float spacing, and never reach the snap threshold.
    static constexpr double kSnapEpsilon = 1.0e-7;
    double coeff_ = 1.0;
    double value_ = 0.0;
    double target_ = 0.0;
};

} // namespace ethernauta
