#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginTestUtils.h"
#include "VoicePanel.h"

#include <ethernauta/JustSnap.h>
#include <ethernauta/Tuning.h>
#include <ethernauta/VoiceParams.h>

#include <set>
#include <string>

using namespace plugintest;
namespace ep = ethernauta::plugin;

TEST_CASE("Plugin exposes six voices through one constant", "[plugin][params]")
{
    CHECK(ep::kNumVoices == 6);
    EthernautaAudioProcessor proc;
    CHECK(proc.getParameters().size() == ep::kTotalParams);
}

TEST_CASE("Parameter order is append-only so index-based host automation survives updates",
          "[plugin][params][order]")
{
    EthernautaAudioProcessor proc;
    const auto& params = proc.getParameters();
    REQUIRE(params.size() == ep::kTotalParams);

    std::vector<std::string> expected;
    // Original layout (commit 27aa24c): wave, latch, gain, pitch for each voice in turn.
    for (int v = 0; v < ep::kNumVoices; ++v)
        for (const char* suffix : {"wave", "latch", "gain", "pitch"}) expected.push_back(voiceId(v, suffix));
    REQUIRE(static_cast<int>(expected.size()) == ep::kNumVoices * ep::kNumBaseVoiceParams);
    // Then the tuning parameters of every voice, then the globals.
    for (int v = 0; v < ep::kNumVoices; ++v)
        for (const char* suffix : {"tuning", "degree", "octave"}) expected.push_back(voiceId(v, suffix));
    expected.push_back(ep::kRootParamId);
    // Then the snap controls (appended after the globals so no earlier index moves).
    for (int v = 0; v < ep::kNumVoices; ++v) expected.push_back(voiceId(v, "snap"));
    expected.push_back(ep::kSnapHysteresisParamId);
    REQUIRE(expected.size() == static_cast<std::size_t>(ep::kTotalParams));

    for (int i = 0; i < params.size(); ++i) {
        const auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*>(params[i]);
        REQUIRE(withId != nullptr);
        INFO("index " << i);
        CHECK(withId->getParameterID().toStdString() == expected[static_cast<std::size_t>(i)]);
    }
}

TEST_CASE("Every voice has its stable parameter IDs", "[plugin][params]")
{
    EthernautaAudioProcessor proc;
    const auto ids = allExpectedIds(ep::kNumVoices);
    REQUIRE(ids.size() == static_cast<std::size_t>(ep::kTotalParams));
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
    for (int v = 0; v < ep::kNumVoices; ++v) {
        CHECK(ep::parameterId(v, ep::VoiceParam::Wave) == voiceId(v, "wave"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Latch) == voiceId(v, "latch"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Gain) == voiceId(v, "gain"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Pitch) == voiceId(v, "pitch"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Tuning) == voiceId(v, "tuning"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Degree) == voiceId(v, "degree"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Octave) == voiceId(v, "octave"));
        CHECK(ep::parameterId(v, ep::VoiceParam::Snap) == voiceId(v, "snap"));
    }
    CHECK(std::string(ep::kRootParamId) == "root");
    CHECK(std::string(ep::kSnapHysteresisParamId) == "snap_hysteresis");
}

TEST_CASE("Tuning parameters expose the documented ranges and defaults", "[plugin][params][tuning]")
{
    EthernautaAudioProcessor proc;

    const auto& root = param(proc, "root");
    CHECK(root.getNormalisableRange().start == ep::kPitchMinNote);
    CHECK(root.getNormalisableRange().end == ep::kPitchMaxNote);
    CHECK(root.getNormalisableRange().start == 12.0f);
    CHECK(root.getNormalisableRange().end == 96.0f);
    CHECK(root.convertFrom0to1(root.getDefaultValue()) == Catch::Approx(45.0f));
    CHECK(raw(proc, "root") == Catch::Approx(45.0f));

    for (int v = 0; v < ep::kNumVoices; ++v) {
        INFO("voice " << v + 1);
        const auto& tuning = param(proc, voiceId(v, "tuning"));
        CHECK(tuning.getNumSteps() == 4);
        CHECK(tuning.convertFrom0to1(tuning.getDefaultValue()) == 0.0f); // Free
        CHECK(raw(proc, voiceId(v, "tuning")) == 0.0f);
        CHECK(tuning.getText(tuning.convertTo0to1(0.0f), 32) == "Free");
        CHECK(tuning.getText(tuning.convertTo0to1(1.0f), 32) == "12-TET");
        CHECK(tuning.getText(tuning.convertTo0to1(2.0f), 32) == "Just");
        CHECK(tuning.getText(tuning.convertTo0to1(3.0f), 32) == "Degree");

        const auto& degree = param(proc, voiceId(v, "degree"));
        CHECK(degree.getNumSteps() == ethernauta::kNumJustDegrees);
        CHECK(degree.convertFrom0to1(degree.getDefaultValue()) == 0.0f); // 1/1
        CHECK(degree.getText(degree.getDefaultValue(), 32).startsWith("1/1"));

        const auto& octave = param(proc, voiceId(v, "octave"));
        CHECK(octave.getNormalisableRange().start == -3.0f);
        CHECK(octave.getNormalisableRange().end == 3.0f);
        CHECK(octave.convertFrom0to1(octave.getDefaultValue()) == 0.0f);
    }
}

TEST_CASE("Existing parameters keep their IDs at their original indices", "[plugin][params][order]")
{
    // The frozen shipped contract, written out literally (not derived from the layout code).
    std::vector<std::string> shipped;
    for (int v = 1; v <= 6; ++v)
        for (const char* suffix : {"wave", "latch", "gain", "pitch"})
            shipped.push_back("v" + std::to_string(v) + "_" + suffix);
    for (int v = 1; v <= 6; ++v)
        for (const char* suffix : {"tuning", "degree", "octave"})
            shipped.push_back("v" + std::to_string(v) + "_" + suffix);
    shipped.push_back("root");
    REQUIRE(shipped.size() == 43);

    EthernautaAudioProcessor proc;
    const auto& params = proc.getParameters();
    REQUIRE(static_cast<std::size_t>(params.size()) >= shipped.size());
    for (std::size_t i = 0; i < shipped.size(); ++i) {
        const auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*>(params[static_cast<int>(i)]);
        REQUIRE(withId != nullptr);
        INFO("index " << i);
        CHECK(withId->getParameterID().toStdString() == shipped[i]);
    }
}

TEST_CASE("Snap parameters expose the documented ranges and defaults", "[plugin][params][snap]")
{
    EthernautaAudioProcessor proc;
    CHECK(ep::kTotalParams == 50);
    CHECK(proc.getParameters().size() == 50);

    for (int v = 0; v < ep::kNumVoices; ++v) {
        INFO("voice " << v + 1);
        const auto& snap = param(proc, voiceId(v, "snap"));
        CHECK(snap.getNormalisableRange().start == 0.0f);
        CHECK(snap.getNormalisableRange().end == 1.0f);
        CHECK(snap.convertFrom0to1(snap.getDefaultValue()) == Catch::Approx(1.0f));
        CHECK(raw(proc, voiceId(v, "snap")) == Catch::Approx(1.0f));
        CHECK(snap.getText(snap.convertTo0to1(0.5f), 32) == "50");
        CHECK(snap.getLabel() == "%");
    }

    const auto& hyst = param(proc, "snap_hysteresis");
    CHECK(hyst.getNormalisableRange().start == 0.0f);
    CHECK(hyst.getNormalisableRange().end == 50.0f);
    CHECK(hyst.convertFrom0to1(hyst.getDefaultValue())
          == Catch::Approx(static_cast<float>(ethernauta::kDefaultSnapHysteresisCents)));
    CHECK(raw(proc, "snap_hysteresis") == Catch::Approx(3.0f));
    CHECK(hyst.getLabel() == "cents");
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
    int panels = 0;
    for (auto* child : editor->getChildren())
        if (dynamic_cast<VoicePanel*>(child) != nullptr) ++panels;
    CHECK(panels == ep::kNumVoices);
}
