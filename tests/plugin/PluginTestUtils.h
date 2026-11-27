#pragma once
// Helpers for the plugin-layer tests. Test code only: allocation is fine here.
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace plugintest {

inline constexpr double kRates[] = {44100.0, 48000.0, 96000.0};
inline constexpr int kBlocks[] = {1, 7, 512, 4096};

// All parameter IDs the plugin must expose, written out literally on purpose: this is the frozen
// public contract, so it must not be derived from the code under test.
inline std::vector<std::string> expectedIds(int numVoices)
{
    std::vector<std::string> ids;
    for (int v = 1; v <= numVoices; ++v)
        for (const char* suffix : {"wave", "latch", "gain", "pitch"})
            ids.push_back("v" + std::to_string(v) + "_" + suffix);
    return ids;
}

inline juce::RangedAudioParameter& param(EthernautaAudioProcessor& p, const std::string& id)
{
    auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p.getValueTreeState().getParameter(id));
    jassert(rp != nullptr);
    return *rp;
}

// Sets a parameter from its plain (denormalised) value, as a host or the UI would.
inline void setPlain(EthernautaAudioProcessor& p, const std::string& id, float plain)
{
    auto& rp = param(p, id);
    rp.setValueNotifyingHost(rp.convertTo0to1(plain));
}

inline float raw(EthernautaAudioProcessor& p, const std::string& id)
{
    return p.getValueTreeState().getRawParameterValue(id)->load();
}

inline std::string voiceId(int voiceIndex, const char* suffix)
{
    return "v" + std::to_string(voiceIndex + 1) + "_" + suffix;
}

inline void prepare(EthernautaAudioProcessor& p, double sampleRate, int block)
{
    p.setRateAndBufferSizeDetails(sampleRate, block);
    p.prepareToPlay(sampleRate, block);
}

struct Rendered {
    std::vector<float> left;
    std::vector<float> right;
};

// Renders `total` samples in blocks of `block` through the stereo processor.
inline Rendered render(EthernautaAudioProcessor& p, int total, int block)
{
    Rendered r;
    juce::AudioBuffer<float> buffer(2, block);
    juce::MidiBuffer midi;
    for (int done = 0; done < total;) {
        const int n = std::min(block, total - done);
        buffer.setSize(2, n, false, false, true);
        // Garbage in: the processor must overwrite, not accumulate onto, the host's buffer.
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), 0.7f, n);
        p.processBlock(buffer, midi);
        r.left.insert(r.left.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + n);
        r.right.insert(r.right.end(), buffer.getReadPointer(1), buffer.getReadPointer(1) + n);
        done += n;
    }
    return r;
}

inline float peak(const std::vector<float>& v, std::size_t from = 0)
{
    float m = 0.0f;
    for (std::size_t i = from; i < v.size(); ++i) m = std::max(m, std::fabs(v[i]));
    return m;
}

inline double rms(const std::vector<float>& v, std::size_t from = 0)
{
    double acc = 0.0;
    for (std::size_t i = from; i < v.size(); ++i) acc += static_cast<double>(v[i]) * v[i];
    return v.size() > from ? std::sqrt(acc / static_cast<double>(v.size() - from)) : 0.0;
}

inline bool allFinite(const std::vector<float>& v)
{
    return std::all_of(v.begin(), v.end(), [](float x) { return std::isfinite(x); });
}

} // namespace plugintest
