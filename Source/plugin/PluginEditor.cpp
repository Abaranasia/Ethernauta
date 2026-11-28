#include "PluginEditor.h"

EthernautaEditor::EthernautaEditor(EthernautaAudioProcessor& processor)
    : juce::AudioProcessorEditor(processor)
{
    // The editor only talks to the parameter tree (attachments); it never touches processor state.
    rootLabel_.setText("Root note (all voices tune to this)", juce::dontSendNotification);
    rootLabel_.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(rootLabel_);
    root_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 110, 24);
    addAndMakeVisible(root_);
    rootAttachment_ = std::make_unique<SliderAttachment>(processor.getValueTreeState(),
                                                         ethernauta::plugin::kRootParamId, root_);

    snapHysteresisLabel_.setText("Snap Hysteresis", juce::dontSendNotification);
    snapHysteresisLabel_.setFont(juce::FontOptions(14.0f));
    addAndMakeVisible(snapHysteresisLabel_);
    snapHysteresis_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    snapHysteresis_.setTextValueSuffix(" cents");
    addAndMakeVisible(snapHysteresis_);
    snapHysteresisAttachment_ = std::make_unique<SliderAttachment>(
        processor.getValueTreeState(), ethernauta::plugin::kSnapHysteresisParamId, snapHysteresis_);

    for (int i = 0; i < kNumPanels; ++i) {
        panels_[static_cast<std::size_t>(i)] = std::make_unique<VoicePanel>(processor.getValueTreeState(), i);
        addAndMakeVisible(*panels_[static_cast<std::size_t>(i)]);
    }
    setSize(kPanelWidth * kNumPanels + 2 * kMargin, kTopBarHeight + kPanelHeight + 2 * kMargin);
}

void EthernautaEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15171c));
}

void EthernautaEditor::resized()
{
    auto area = getLocalBounds().reduced(kMargin);
    auto topBar = area.removeFromTop(kTopBarHeight);
    rootLabel_.setBounds(topBar.removeFromLeft(260));
    root_.setBounds(topBar.removeFromLeft(360));
    snapHysteresisLabel_.setBounds(topBar.removeFromLeft(120));
    snapHysteresis_.setBounds(topBar);
    for (int i = 0; i < kNumPanels; ++i)
        panels_[static_cast<std::size_t>(i)]->setBounds(area.removeFromLeft(kPanelWidth));
}
