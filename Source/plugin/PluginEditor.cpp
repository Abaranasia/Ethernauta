#include "PluginEditor.h"

EthernautaEditor::EthernautaEditor(EthernautaAudioProcessor& processor)
    : juce::AudioProcessorEditor(processor)
{
    // The editor only hands the parameter tree to the panels; it never touches processor state.
    for (int i = 0; i < kNumPanels; ++i) {
        panels_[static_cast<std::size_t>(i)] = std::make_unique<VoicePanel>(processor.getValueTreeState(), i);
        addAndMakeVisible(*panels_[static_cast<std::size_t>(i)]);
    }
    setSize(kPanelWidth * kNumPanels + 2 * kMargin, kPanelHeight + 2 * kMargin);
}

void EthernautaEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15171c));
}

void EthernautaEditor::resized()
{
    auto area = getLocalBounds().reduced(kMargin);
    for (int i = 0; i < kNumPanels; ++i)
        panels_[static_cast<std::size_t>(i)]->setBounds(area.removeFromLeft(kPanelWidth));
}
