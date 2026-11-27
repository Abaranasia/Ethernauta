#include "ParameterLayout.h"

#include <ethernauta/VoiceParams.h>

#include <memory>

namespace ethernauta::plugin {

std::string parameterId(int voiceIndex, VoiceParam which)
{
    const char* suffix = "wave";
    switch (which) {
        case VoiceParam::Wave: suffix = "wave"; break;
        case VoiceParam::Latch: suffix = "latch"; break;
        case VoiceParam::Gain: suffix = "gain"; break;
        case VoiceParam::Pitch: suffix = "pitch"; break;
    }
    return "v" + std::to_string(voiceIndex + 1) + "_" + suffix;
}

void addVoiceParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int voiceIndex)
{
    const juce::String voiceName = "Voice " + juce::String(voiceIndex + 1);

    juce::StringArray waveforms;
    for (int i = 0; i < kNumWaveforms; ++i)
        waveforms.add(waveformName(waveformFromIndex(i)));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{parameterId(voiceIndex, VoiceParam::Wave), kParameterVersion},
        voiceName + " Waveform", waveforms, 0));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{parameterId(voiceIndex, VoiceParam::Latch), kParameterVersion},
        voiceName + " Latch", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterId(voiceIndex, VoiceParam::Gain), kParameterVersion},
        voiceName + " Gain",
        juce::NormalisableRange<float>(kMinGainDb, kGainParamMaxDb, 0.1f, 1.6f), VoiceParams{}.gainDb,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float v, int) {
                return v <= kMinGainDb ? juce::String("-inf") : juce::String(v, 1);
            })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{parameterId(voiceIndex, VoiceParam::Pitch), kParameterVersion},
        voiceName + " Pitch",
        juce::NormalisableRange<float>(kPitchMinNote, kPitchMaxNote, 0.01f), defaultPitchNote(voiceIndex),
        juce::AudioParameterFloatAttributes()
            .withLabel("note")
            .withStringFromValueFunction([](float v, int) {
                // Number first so the default text-entry parser (leading number) round-trips.
                return juce::String(v, 2) + " ("
                       + juce::MidiMessage::getMidiNoteName(juce::roundToInt(v), true, true, 4) + ")";
            })));
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int voice = 0; voice < kNumVoices; ++voice)
        addVoiceParameters(layout, voice);
    return layout;
}

} // namespace ethernauta::plugin
