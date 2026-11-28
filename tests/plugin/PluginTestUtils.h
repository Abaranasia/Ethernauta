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

// Parameters added with the tuning engine (state schema 2): the global "root" plus three per voice.
inline std::vector<std::string> expectedTuningIds(int numVoices)
{
    std::vector<std::string> ids{"root"};
    for (int v = 1; v <= numVoices; ++v)
        for (const char* suffix : {"tuning", "degree", "octave"})
            ids.push_back("v" + std::to_string(v) + "_" + suffix);
    return ids;
}

// Parameters added with the snap controls (state schema 3): one strength per voice, then the global
// hysteresis, appended after "root".
inline std::vector<std::string> expectedSnapIds(int numVoices)
{
    std::vector<std::string> ids;
    for (int v = 1; v <= numVoices; ++v) ids.push_back("v" + std::to_string(v) + "_snap");
    ids.push_back("snap_hysteresis");
    return ids;
}

inline std::vector<std::string> allExpectedIds(int numVoices)
{
    auto ids = expectedIds(numVoices);
    const auto tuning = expectedTuningIds(numVoices);
    ids.insert(ids.end(), tuning.begin(), tuning.end());
    const auto snap = expectedSnapIds(numVoices);
    ids.insert(ids.end(), snap.begin(), snap.end());
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

// Frequency from positive-going zero crossings with linear interpolation of each crossing time.
// Sub-sample accurate, so the error is far below one semitone even for a low note in a short window.
inline double estimateFrequencyHz(const std::vector<float>& v, double sampleRate, std::size_t from = 0)
{
    double first = 0.0, last = 0.0;
    int crossings = 0;
    for (std::size_t i = from + 1; i < v.size(); ++i) {
        if (v[i - 1] < 0.0f && v[i] >= 0.0f) {
            const double t = static_cast<double>(i - 1)
                             + static_cast<double>(-v[i - 1]) / static_cast<double>(v[i] - v[i - 1]);
            if (crossings++ == 0) first = t;
            last = t;
        }
    }
    return crossings < 2 ? 0.0 : (crossings - 1) * sampleRate / (last - first);
}

// Peak / RMS: ~1.414 for a sine, ~1.0 for a square.
inline double crestFactor(const std::vector<float>& v, std::size_t from = 0)
{
    const double r = rms(v, from);
    return r > 0.0 ? static_cast<double>(peak(v, from)) / r : 0.0;
}

inline double midiNoteToHz(double note)
{
    return 440.0 * std::pow(2.0, (note - 69.0) / 12.0);
}

inline bool allFinite(const std::vector<float>& v)
{
    return std::all_of(v.begin(), v.end(), [](float x) { return std::isfinite(x); });
}

} // namespace plugintest
