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

    const char* const choirIds[ethernauta::plugin::kNumChoirs] = {ethernauta::plugin::kChoir1ChordParamId,
                                                                    ethernauta::plugin::kChoir2ChordParamId};
    for (std::size_t c = 0; c < choirLabels_.size(); ++c) {
        choirLabels_[c].setText("Choir " + juce::String(static_cast<int>(c) + 1), juce::dontSendNotification);
        choirLabels_[c].setFont(juce::FontOptions(14.0f));
        addAndMakeVisible(choirLabels_[c]);
        // Item IDs are 1-based; the attachment maps them to the choice indices (Major first).
        choirChords_[c].addItemList(juce::StringArray{"Major", "Minor", "Septimal Minor", "Supermajor"}, 1);
        addAndMakeVisible(choirChords_[c]);
        choirChordAttachments_[c] =
            std::make_unique<ComboAttachment>(processor.getValueTreeState(), choirIds[c], choirChords_[c]);
    }

    for (int i = 0; i < kNumPanels; ++i) {
        panels_[static_cast<std::size_t>(i)] = std::make_unique<VoicePanel>(processor.getValueTreeState(), i);
        addAndMakeVisible(*panels_[static_cast<std::size_t>(i)]);
    }
    setSize(kPanelWidth * kNumPanels + 2 * kMargin, kTopBarHeight + kChoirBarHeight + kPanelHeight + 2 * kMargin);
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
    auto choirBar = area.removeFromTop(kChoirBarHeight);
    for (std::size_t c = 0; c < choirLabels_.size(); ++c) {
        choirLabels_[c].setBounds(choirBar.removeFromLeft(70));
        choirChords_[c].setBounds(choirBar.removeFromLeft(160).reduced(0, 4));
        choirBar.removeFromLeft(20);
    }
    for (int i = 0; i < kNumPanels; ++i)
        panels_[static_cast<std::size_t>(i)]->setBounds(area.removeFromLeft(kPanelWidth));
}
