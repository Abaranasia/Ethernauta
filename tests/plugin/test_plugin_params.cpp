#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginTestUtils.h"

#include <ethernauta/VoiceParams.h>

#include <set>

using namespace plugintest;
namespace ep = ethernauta::plugin;

TEST_CASE("Plugin exposes six voices through one constant", "[plugin][params]")
{
    CHECK(ep::kNumVoices == 6);
    EthernautaAudioProcessor proc;
    CHECK(proc.getParameters().size() == ep::kNumVoices * 4);
}

TEST_CASE("Every voice has its stable parameter IDs", "[plugin][params]")
{
    EthernautaAudioProcessor proc;
    const auto ids = expectedIds(6);
    REQUIRE(ids.size() == 24);
    for (const auto& id : ids) {
        INFO(id);
        CHECK(proc.getValueTreeState().getParameter(id) != nullptr);
    }
    // No other IDs: the layout contains exactly the expected set.
    std::set<std::string> expected(ids.begin(), ids.end());
    for (auto* p : proc.getParameters()) {
        const auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*>(p);
        REQUIRE(withId != nullptr);
        CHECK(expected.count(withId->getParameterID().toStdString()) == 1);
    }
    // The builder agrees with the literal contract.
    for (int v = 0; v < 6; ++v) {
        CHECK(ep::parameterId(v, ep::VoiceParam::Wave) == voiceId(v, "wave"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Latch) == voiceId(v, "latch"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Gain) == voiceId(v, "gain"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Pitch) == voiceId(v, "pitch"));
    }
}

TEST_CASE("Voice 1 parameters keep their shipped ranges and defaults", "[plugin][params]")
{
    EthernautaAudioProcessor proc;
    const ethernauta::VoiceParams core;

    const auto& gain = param(proc, "v1_gain");
    CHECK(gain.getNormalisableRange().start == ethernauta::kMinGainDb);
    CHECK(gain.getNormalisableRange().start == -60.0f);
    CHECK(gain.getNormalisableRange().end == 6.0f);
    CHECK(gain.convertFrom0to1(gain.getDefaultValue()) == Catch::Approx(-12.0f));
    CHECK(gain.convertFrom0to1(gain.getDefaultValue()) == Catch::Approx(core.gainDb));

    const auto& pitch = param(proc, "v1_pitch");
    CHECK(pitch.getNormalisableRange().start == 12.0f);
    CHECK(pitch.getNormalisableRange().end == 96.0f);
    CHECK(pitch.convertFrom0to1(pitch.getDefaultValue()) == Catch::Approx(45.0f));
    CHECK(pitch.convertFrom0to1(pitch.getDefaultValue()) == Catch::Approx(core.midiNote));
}

TEST_CASE("Layout ranges and defaults match the core for every voice", "[plugin][params]")
{
    EthernautaAudioProcessor proc;
    const ethernauta::VoiceParams core;
    // Open A-major stack above A2 (documented in ParameterLayout.h).
    const float expectedPitch[6] = {45.0f, 52.0f, 57.0f, 61.0f, 64.0f, 69.0f};

    for (int v = 0; v < 6; ++v) {
        INFO("voice " << v + 1);
        const auto& wave = param(proc, voiceId(v, "wave"));
        CHECK(wave.getNumSteps() == ethernauta::kNumWaveforms);
        CHECK(wave.convertFrom0to1(wave.getDefaultValue()) == static_cast<float>(core.waveform));

        const auto& latch = param(proc, voiceId(v, "latch"));
        CHECK(latch.getDefaultValue() == (core.latched ? 1.0f : 0.0f));

        const auto& gain = param(proc, voiceId(v, "gain"));
        CHECK(gain.getNormalisableRange().start == ethernauta::kMinGainDb);
        CHECK(gain.getNormalisableRange().end <= ethernauta::kMaxGainDb);
        CHECK(gain.convertFrom0to1(gain.getDefaultValue()) == Catch::Approx(core.gainDb));

        const auto& pitch = param(proc, voiceId(v, "pitch"));
        CHECK(pitch.getNormalisableRange().start == ep::kPitchMinNote);
        CHECK(pitch.getNormalisableRange().end == ep::kPitchMaxNote);
        CHECK(pitch.convertFrom0to1(pitch.getDefaultValue()) == Catch::Approx(expectedPitch[v]));
        CHECK(ep::defaultPitchNote(v) == expectedPitch[v]);

        // Fresh processor starts at the defaults and every voice is unlatched.
        CHECK(raw(proc, voiceId(v, "latch")) == 0.0f);
        CHECK(raw(proc, voiceId(v, "pitch")) == Catch::Approx(expectedPitch[v]));
    }
}

TEST_CASE("Default pitches are distinct and ascending", "[plugin][params]")
{
    for (int v = 1; v < ep::kNumVoices; ++v) CHECK(ep::defaultPitchNote(v) > ep::defaultPitchNote(v - 1));
}

TEST_CASE("Setting one voice's pitch changes only that voice's stored parameters", "[plugin][params][independence]")
{
    EthernautaAudioProcessor proc;
    constexpr int changed = 3;
    setPlain(proc, voiceId(changed, "pitch"), 70.0f);
    CHECK(raw(proc, voiceId(changed, "pitch")) == Catch::Approx(70.0f));
    for (int v = 0; v < ep::kNumVoices; ++v) {
        if (v == changed) continue;
        CHECK(raw(proc, voiceId(v, "pitch")) == Catch::Approx(ep::defaultPitchNote(v)));
        CHECK(raw(proc, voiceId(v, "gain")) == Catch::Approx(ethernauta::VoiceParams{}.gainDb));
    }
}

TEST_CASE("Editor can be created at a fixed size", "[plugin][editor]")
{
    EthernautaAudioProcessor proc;
    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    REQUIRE(editor != nullptr);
    CHECK(editor->getWidth() >= 6 * 140);
    CHECK(editor->getHeight() > 200);
    // One panel per voice, laid out left to right.
    CHECK(editor->getNumChildComponents() == ep::kNumVoices);
}
