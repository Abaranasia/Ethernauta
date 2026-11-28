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
// 1: four parameters per voice. 2: tuning engine (global "root", per-voice tuning/degree/octave).
// 3: snap controls (per-voice "snap" strength, global "snap_hysteresis").
inline constexpr int kStateSchemaVersion = 3;

// Version hint attached to every parameter. Parameter IDs are stable strings and must never change
// once shipped (hosts store automation against them).
inline constexpr int kParameterVersion = 1;
// Parameters added after the first release carry a higher hint (used by AU hosts).
inline constexpr int kTuningParameterVersion = 2;
inline constexpr int kSnapParameterVersion = 3;

enum class VoiceParam { Wave, Latch, Gain, Pitch, Tuning, Degree, Octave, Snap };

// Parameter counts. Host automation may address parameters by index, so the layout order is
// append-only: [wave, latch, gain, pitch] for voices 1..N (the original layout), then
// [tuning, degree, octave] for voices 1..N, then the global [root], then [snap] for voices 1..N and
// the global [snap_hysteresis]. New parameters go at the end, never in between, and existing ones
// are never reordered.
inline constexpr int kNumBaseVoiceParams = 4;   // wave, latch, gain, pitch
inline constexpr int kNumTuningVoiceParams = 3; // tuning, degree, octave
inline constexpr int kNumSnapVoiceParams = 1;   // snap
inline constexpr int kNumVoiceParams = kNumBaseVoiceParams + kNumTuningVoiceParams + kNumSnapVoiceParams;
inline constexpr int kNumGlobalParams = 2;      // root, snap_hysteresis
inline constexpr int kTotalParams = kNumVoices * kNumVoiceParams + kNumGlobalParams;

// Stable parameter ID for a zero-based voice index: "v1_wave", "v1_latch", "v1_gain", "v1_pitch",
// "v1_tuning", "v1_degree", "v1_octave", "v1_snap", ...
std::string parameterId(int voiceIndex, VoiceParam which);

// Global tuning root (MIDI note, whole notes, same range as the voice pitch).
inline constexpr const char* kRootParamId = "root";
inline constexpr float kRootDefaultNote = 45.0f; // A2 = 110 Hz

// Global snap hysteresis for every voice's Just snapping, in cents (see VoiceTuning). The default is
// the core's kDefaultSnapHysteresisCents.
inline constexpr const char* kSnapHysteresisParamId = "snap_hysteresis";
inline constexpr float kSnapHysteresisMaxCents = 50.0f;

// UI/automation range of the pitch parameter (MIDI notes); the core accepts a wider range.
inline constexpr float kPitchMinNote = 12.0f;
inline constexpr float kPitchMaxNote = 96.0f;

// Appends the tuning root, the first global parameter.
void addGlobalParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

// Appends the snap parameters (one strength per voice, then the global hysteresis). They come after
// the tuning globals so no earlier parameter index moves.
void addSnapParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout);

// Appends the original four parameters of one voice (wave, latch, gain, pitch).
void addVoiceBaseParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int voiceIndex);

// Appends the tuning parameters of one voice (tuning, degree, octave).
void addVoiceTuningParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int voiceIndex);

// Builds the layout in the append-only order documented at kNumBaseVoiceParams.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace ethernauta::plugin
