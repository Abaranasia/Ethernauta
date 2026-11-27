#pragma once
// Shared helpers for the DSP tests. Test code only: allocation is fine here.
#include <ethernauta/DroneVoice.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace testutil {

inline constexpr double kRates[] = {44100.0, 48000.0, 96000.0};

inline int secondsToSamples(double seconds, double sampleRate)
{
    return static_cast<int>(seconds * sampleRate);
}

// Renders `total` samples through a voice in blocks of `block` samples (additive into a zeroed buffer).
inline std::vector<float> render(ethernauta::DroneVoice& voice, int total, int block)
{
    std::vector<float> out(static_cast<std::size_t>(total), 0.0f);
    for (int done = 0; done < total;) {
        const int n = std::min(block, total - done);
        voice.process(out.data() + done, n);
        done += n;
    }
    return out;
}

inline bool allFinite(const std::vector<float>& v)
{
    return std::all_of(v.begin(), v.end(), [](float x) { return std::isfinite(x); });
}

inline float peak(const std::vector<float>& v, std::size_t from = 0)
{
    float p = 0.0f;
    for (std::size_t i = from; i < v.size(); ++i) p = std::max(p, std::fabs(v[i]));
    return p;
}

inline double rms(const std::vector<float>& v, std::size_t from = 0)
{
    double acc = 0.0;
    for (std::size_t i = from; i < v.size(); ++i) acc += static_cast<double>(v[i]) * v[i];
    return v.size() > from ? std::sqrt(acc / static_cast<double>(v.size() - from)) : 0.0;
}

inline float maxStep(const std::vector<float>& v, std::size_t from = 1)
{
    float m = 0.0f;
    for (std::size_t i = std::max<std::size_t>(from, 1); i < v.size(); ++i)
        m = std::max(m, std::fabs(v[i] - v[i - 1]));
    return m;
}

// Frequency estimate from upward zero crossings with linear interpolation of the crossing instants:
// (crossings - 1) / (time between first and last crossing). Returns 0 when fewer than 3 crossings.
inline double estimateFrequency(const std::vector<float>& v, double sampleRate, std::size_t from = 0)
{
    double first = 0.0;
    double last = 0.0;
    int count = 0;
    for (std::size_t i = std::max<std::size_t>(from, 1); i < v.size(); ++i) {
        if (v[i - 1] < 0.0f && v[i] >= 0.0f) {
            const double frac = static_cast<double>(-v[i - 1]) / (static_cast<double>(v[i]) - v[i - 1]);
            const double t = (static_cast<double>(i - 1) + frac) / sampleRate;
            if (count == 0) first = t;
            last = t;
            ++count;
        }
    }
    return count >= 3 ? (count - 1) / (last - first) : 0.0;
}

} // namespace testutil
