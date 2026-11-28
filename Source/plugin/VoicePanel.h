#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// UI for ONE drone voice. It only talks to the parameter tree through attachments; it holds no
// reference to the processor. Instantiate it once per voice with the zero-based voice index.
class VoicePanel : public juce::Component {
public:
    VoicePanel(juce::AudioProcessorValueTreeState& apvts, int voiceIndex);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    juce::Label title_;
    juce::ComboBox waveform_;
    juce::ToggleButton latch_{"Latch"};
    // Tuning: mode and degree are choice parameters, the octave an integer slider. The pitch dial stays
    // enabled in every mode: Free, 12-TET and Just use it as the value to tune; only Degree ignores it.
    juce::ComboBox tuning_;
    juce::ComboBox degree_;
    juce::Slider octave_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::Label octaveLabel_;
    juce::Slider gain_{juce::Slider::LinearVertical, juce::Slider::TextBoxBelow};
    juce::Slider pitch_{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label gainLabel_;
    juce::Label pitchLabel_;

    // Declared last: attachments must be destroyed before the controls they point at.
    std::unique_ptr<ComboAttachment> waveformAttachment_;
    std::unique_ptr<ButtonAttachment> latchAttachment_;
    std::unique_ptr<ComboAttachment> tuningAttachment_;
    std::unique_ptr<ComboAttachment> degreeAttachment_;
    std::unique_ptr<SliderAttachment> octaveAttachment_;
    std::unique_ptr<SliderAttachment> gainAttachment_;
    std::unique_ptr<SliderAttachment> pitchAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoicePanel)
};
