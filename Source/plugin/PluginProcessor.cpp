#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <ethernauta/MasterStage.h>

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
    }
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
    float* left = buffer.getWritePointer(0);
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        const auto& p = params_[i];
        ethernauta::VoiceParams vp;
        vp.waveform = ethernauta::waveformFromIndex(static_cast<int>(std::lround(p.wave->load(std::memory_order_relaxed))));
        vp.latched = p.latch->load(std::memory_order_relaxed) >= 0.5f;
        vp.gainDb = p.gain->load(std::memory_order_relaxed);
        vp.midiNote = p.pitch->load(std::memory_order_relaxed);
        voices_[i].setParams(vp);
        voices_[i].process(left, numSamples);
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
    // Migration hook: states older than kStateSchemaVersion would be upgraded here. Version 1 is
    // the first schema, so there is nothing to migrate yet. Newer versions are loaded best-effort:
    // unknown properties are ignored by the APVTS.
    (void) savedVersion;

    apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    apvts_.state.setProperty(kSchemaVersionKey, ethernauta::plugin::kStateSchemaVersion, nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EthernautaAudioProcessor();
}
