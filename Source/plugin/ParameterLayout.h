#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include <string>

namespace ethernauta::plugin {

// Number of drone voices the plugin currently instantiates. Raising this to 6 is the only change
// the layout, processor and editor loops need.
inline constexpr int kNumVoices = 1;
// Number of panel slots the editor is designed for.
inline constexpr int kMaxVoices = 6;

// Bump when the saved state layout changes, and add a migration branch in setStateInformation.
inline constexpr int kStateSchemaVersion = 1;

// Version hint attached to every parameter. Parameter IDs are stable strings and must never change
// once shipped (hosts store automation against them).
inline constexpr int kParameterVersion = 1;

enum class VoiceParam { Wave, Latch, Gain, Pitch };

// Stable parameter ID for a zero-based voice index: "v1_wave", "v1_latch", "v1_gain", "v1_pitch", ...
std::string parameterId(int voiceIndex, VoiceParam which);

// UI/automation range of the pitch parameter (MIDI notes); the core accepts a wider range.
inline constexpr float kPitchMinNote = 12.0f;
inline constexpr float kPitchMaxNote = 96.0f;

// Appends the four parameters of one voice. Called in a loop over voices by createParameterLayout().
void addVoiceParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int voiceIndex);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace ethernauta::plugin
