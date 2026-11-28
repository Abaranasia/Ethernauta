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
    static constexpr int kPanelWidth = 160;
    static constexpr int kPanelHeight = 500;
    static constexpr int kTopBarHeight = 40;
    static constexpr int kMargin = 10;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    juce::Label rootLabel_;
    juce::Slider root_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label snapHysteresisLabel_;
    juce::Slider snapHysteresis_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    std::array<std::unique_ptr<VoicePanel>, kNumPanels> panels_;
    std::unique_ptr<SliderAttachment> rootAttachment_; // after the slider it points at
    std::unique_ptr<SliderAttachment> snapHysteresisAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EthernautaEditor)
};
