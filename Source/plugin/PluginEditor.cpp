#include "PluginEditor.h"

EthernautaEditor::EthernautaEditor(EthernautaAudioProcessor& processor)
    : juce::AudioProcessorEditor(processor)
{
    // The editor only hands the parameter tree to the panels; it never touches processor state.
    for (int i = 0; i < kVisiblePanels; ++i) {
        panels_[static_cast<std::size_t>(i)] = std::make_unique<VoicePanel>(processor.getValueTreeState(), i);
        addAndMakeVisible(*panels_[static_cast<std::size_t>(i)]);
    }
    setSize(kPanelWidth * kVisiblePanels + 20, kPanelHeight + 20);
}

void EthernautaEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15171c));
}

void EthernautaEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    for (int i = 0; i < kVisiblePanels; ++i)
        panels_[static_cast<std::size_t>(i)]->setBounds(area.removeFromLeft(kPanelWidth));
}
