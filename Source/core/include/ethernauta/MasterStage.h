#pragma once
#include <cmath>

namespace ethernauta {

// Output stage applied once to the summed voices, so any number of simultaneous voices (six latched
// voices at full gain sum to well above 1.0) can never exceed full scale.
//
// Choice: a fixed trim of -6 dB followed by tanh. The trim leaves the default level (-12 dB voices)
// in the near-linear region of tanh (~0.5% deviation at the default-level peak: 0.251 * 0.5 = 0.126,
// and 1 - tanh(x)/x ~= x^2/3); tanh is smooth, odd and
// strictly bounded by 1, so overload saturates softly instead of hard clipping. It maps 0 to
// exactly 0, keeping silence bit-exact. Per-sample, stateless, allocation-free.
inline constexpr float kMasterTrim = 0.5f;

inline float masterStage(float x) noexcept
{
    return std::tanh(x * kMasterTrim);
}

inline void applyMasterStage(float* buffer, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i) buffer[i] = masterStage(buffer[i]);
}

} // namespace ethernauta
