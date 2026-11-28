#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <ethernauta/MasterStage.h>

#include <algorithm>
#include <cmath>

namespace {
const juce::Identifier kSchemaVersionKey{"schemaVersion"};
} // namespace

EthernautaAudioProcessor::EthernautaAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "EthernautaState", ethernauta::plugin::createParameterLayout())
{
    using ethernauta::plugin::parameterId;
    using ethernauta::plugin::VoiceParam;
    for (int i = 0; i < ethernauta::plugin::kNumVoices; ++i) {
        auto& p = params_[static_cast<std::size_t>(i)];
        p.wave = apvts_.getRawParameterValue(parameterId(i, VoiceParam::Wave));
        p.latch = apvts_.getRawParameterValue(parameterId(i, VoiceParam::Latch));
        p.gain = apvts_.getRawParameterValue(parameterId(i, VoiceParam::Gain));
        p.pitch = apvts_.getRawParameterValue(parameterId(i, VoiceParam::Pitch));
        p.tuning = apvts_.getRawParameterValue(parameterId(i, VoiceParam::Tuning));
        p.degree = apvts_.getRawParameterValue(parameterId(i, VoiceParam::Degree));
        p.octave = apvts_.getRawParameterValue(parameterId(i, VoiceParam::Octave));
    }
    root_ = apvts_.getRawParameterValue(ethernauta::plugin::kRootParamId);
    apvts_.state.setProperty(kSchemaVersionKey, ethernauta::plugin::kStateSchemaVersion, nullptr);
}

void EthernautaAudioProcessor::prepareToPlay(double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
{
    for (auto& v : voices_)
        v.prepare(sampleRate);
}

bool EthernautaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet().isDisabled()
           && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void EthernautaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels <= 0 || numSamples <= 0) return;

    // Voices render additively into channel 0, which is mirrored to the other channels afterwards.
    // Every voice is tuned from the one shared root; the voice glides to the resulting frequency.
    tuner_.setRoot(static_cast<double>(root_->load(std::memory_order_relaxed)));
    float* left = buffer.getWritePointer(0);
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        const auto& p = params_[i];
        ethernauta::VoiceTuning vt;
        vt.mode = static_cast<ethernauta::TuningMode>(std::clamp(
            static_cast<int>(std::lround(p.tuning->load(std::memory_order_relaxed))), 0, ethernauta::kNumTuningModes - 1));
        vt.degree = static_cast<int>(std::lround(p.degree->load(std::memory_order_relaxed)));
        vt.octave = static_cast<int>(std::lround(p.octave->load(std::memory_order_relaxed)));
        const double freeHz = ethernauta::pitchToHz(p.pitch->load(std::memory_order_relaxed), 0.0f);

        auto& voice = voices_[i];
        voice.setWaveform(ethernauta::waveformFromIndex(static_cast<int>(std::lround(p.wave->load(std::memory_order_relaxed)))));
        voice.setGainDb(p.gain->load(std::memory_order_relaxed));
        voice.setFrequencyHz(tuner_.targetHz(static_cast<int>(i), vt, freeHz));
        voice.setLatched(p.latch->load(std::memory_order_relaxed) >= 0.5f); // last: the rest is final when the gate opens
        voice.process(left, numSamples);
    }
    // Headroom safeguard for the additive mix of all voices (see MasterStage.h).
    ethernauta::applyMasterStage(left, numSamples);
    for (int ch = 1; ch < numChannels; ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* EthernautaAudioProcessor::createEditor()
{
    return new EthernautaEditor(*this);
}

void EthernautaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty(kSchemaVersionKey, ethernauta::plugin::kStateSchemaVersion, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void EthernautaAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts_.state.getType())) return;

    const int savedVersion = xml->getIntAttribute(kSchemaVersionKey.toString(), 0);
    apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    // Migration: schema 1 predates the tuning engine, so its parameters take their defaults (Free
    // tuning, i.e. the pre-tuning behaviour). Newer versions are loaded best-effort: unknown
    // properties are ignored by the APVTS.
    if (savedVersion < 2) resetTuningParametersToDefaults();
    apvts_.state.setProperty(kSchemaVersionKey, ethernauta::plugin::kStateSchemaVersion, nullptr);
}

void EthernautaAudioProcessor::resetTuningParametersToDefaults()
{
    using ethernauta::plugin::parameterId;
    using ethernauta::plugin::VoiceParam;
    auto reset = [this](const std::string& id) {
        if (auto* p = apvts_.getParameter(id)) p->setValueNotifyingHost(p->getDefaultValue());
    };
    reset(ethernauta::plugin::kRootParamId);
    for (int i = 0; i < ethernauta::plugin::kNumVoices; ++i)
        for (const auto which : {VoiceParam::Tuning, VoiceParam::Degree, VoiceParam::Octave})
            reset(parameterId(i, which));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EthernautaAudioProcessor();
}
