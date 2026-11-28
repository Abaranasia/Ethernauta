#include "VoicePanel.h"

#include "ParameterLayout.h"

#include <ethernauta/Tuning.h>
#include <ethernauta/TuningEngine.h>
#include <ethernauta/Waveform.h>

namespace {
void setupCaption(juce::Component& parent, juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(12.0f));
    parent.addAndMakeVisible(label);
}
} // namespace

VoicePanel::VoicePanel(juce::AudioProcessorValueTreeState& apvts, int voiceIndex)
{
    using ethernauta::plugin::parameterId;
    using ethernauta::plugin::VoiceParam;

    title_.setText("Voice " + juce::String(voiceIndex + 1), juce::dontSendNotification);
    title_.setJustificationType(juce::Justification::centred);
    title_.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    addAndMakeVisible(title_);

    // Item IDs are 1-based (0 means "no selection"); the attachment maps them to choice indices.
    for (int i = 0; i < ethernauta::kNumWaveforms; ++i)
        waveform_.addItem(ethernauta::waveformName(ethernauta::waveformFromIndex(i)), i + 1);
    addAndMakeVisible(waveform_);
    addAndMakeVisible(latch_);

    tuning_.addItemList(juce::StringArray{"Free", "12-TET", "Just", "Degree"}, 1);
    addAndMakeVisible(tuning_);
    for (int i = 0; i < ethernauta::kNumJustDegrees; ++i) {
        const auto& d = ethernauta::kJustTable[i];
        degree_.addItem(juce::String(d.num) + "/" + juce::String(d.den) + " " + d.name, i + 1);
    }
    addAndMakeVisible(degree_);
    octave_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 20);
    addAndMakeVisible(octave_);
    setupCaption(*this, octaveLabel_, "Oct");

    gain_.setTextValueSuffix(" dB");
    addAndMakeVisible(gain_);
    pitch_.setTextValueSuffix("");
    addAndMakeVisible(pitch_);
    setupCaption(*this, gainLabel_, "Gain");
    setupCaption(*this, pitchLabel_, "Pitch");

    waveformAttachment_ = std::make_unique<ComboAttachment>(apvts, parameterId(voiceIndex, VoiceParam::Wave), waveform_);
    latchAttachment_ = std::make_unique<ButtonAttachment>(apvts, parameterId(voiceIndex, VoiceParam::Latch), latch_);
    gainAttachment_ = std::make_unique<SliderAttachment>(apvts, parameterId(voiceIndex, VoiceParam::Gain), gain_);
    pitchAttachment_ = std::make_unique<SliderAttachment>(apvts, parameterId(voiceIndex, VoiceParam::Pitch), pitch_);
    tuningAttachment_ = std::make_unique<ComboAttachment>(apvts, parameterId(voiceIndex, VoiceParam::Tuning), tuning_);
    degreeAttachment_ = std::make_unique<ComboAttachment>(apvts, parameterId(voiceIndex, VoiceParam::Degree), degree_);
    octaveAttachment_ = std::make_unique<SliderAttachment>(apvts, parameterId(voiceIndex, VoiceParam::Octave), octave_);
}

void VoicePanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 6.0f);
}

void VoicePanel::resized()
{
    auto area = getLocalBounds().reduced(10);
    title_.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    waveform_.setBounds(area.removeFromTop(26));
    area.removeFromTop(6);
    latch_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    tuning_.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);
    degree_.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);
    auto octaveRow = area.removeFromTop(24);
    octaveLabel_.setBounds(octaveRow.removeFromLeft(30));
    octave_.setBounds(octaveRow);
    area.removeFromTop(8);

    auto pitchArea = area.removeFromBottom(120);
    pitchLabel_.setBounds(pitchArea.removeFromTop(16));
    pitch_.setBounds(pitchArea);
    area.removeFromBottom(8);

    gainLabel_.setBounds(area.removeFromTop(16));
    gain_.setBounds(area);
}
