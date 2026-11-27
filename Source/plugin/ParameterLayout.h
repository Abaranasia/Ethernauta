#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include <string>

namespace ethernauta::plugin {

// Number of independent drone voices: the single source of truth for the parameter layout, the
// processor's voice array and the editor's panels.
inline constexpr int kNumVoices = 6;

// Default pitch (MIDI note) per voice: an open A-major stack above A2 (A2 E3 A3 C#4 E4 A4), so all
// six voices latched together form a consonant chord. Voice 1 equals VoiceParams{}.midiNote (45).
// Changing the entry of a shipped voice changes only its default, never its ID or range.
inline constexpr float kDefaultPitchNotes[] = {45.0f, 52.0f, 57.0f, 61.0f, 64.0f, 69.0f};
static_assert(sizeof(kDefaultPitchNotes) / sizeof(float) == kNumVoices, "one default pitch per voice");
inline constexpr float defaultPitchNote(int voiceIndex) noexcept { return kDefaultPitchNotes[voiceIndex]; }

// Gain parameter range (dB). The lower bound is the core's mute floor; the upper bound stays below
// the core's own maximum.
inline constexpr float kGainParamMaxDb = 6.0f;

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
