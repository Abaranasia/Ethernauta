#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

#include "ParameterLayout.h"
#include "PluginProcessor.h"
#include "VoicePanel.h"

class EthernautaEditor : public juce::AudioProcessorEditor {
public:
    explicit EthernautaEditor(EthernautaAudioProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kNumPanels = ethernauta::plugin::kNumVoices;
    static constexpr int kPanelWidth = 150;
    static constexpr int kPanelHeight = 420;
    static constexpr int kMargin = 10;

    std::array<std::unique_ptr<VoicePanel>, kNumPanels> panels_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EthernautaEditor)
};
