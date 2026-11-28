#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginTestUtils.h"

using namespace plugintest;
namespace ep = ethernauta::plugin;

namespace {
juce::MemoryBlock saveState(EthernautaAudioProcessor& p)
{
    juce::MemoryBlock mb;
    p.getStateInformation(mb);
    return mb;
}

void expectAllDefaults(EthernautaAudioProcessor& p)
{
    EthernautaAudioProcessor fresh;
    for (const auto& id : allExpectedIds(ep::kNumVoices)) {
        INFO(id);
        CHECK(raw(p, id) == raw(fresh, id));
    }
}
} // namespace

TEST_CASE("State round-trips every parameter of every voice", "[plugin][state]")
{
    EthernautaAudioProcessor a;
    EthernautaAudioProcessor defaults;
    for (int v = 0; v < ep::kNumVoices; ++v) {
        setPlain(a, voiceId(v, "wave"), static_cast<float>(v % 3 + 1));
        setPlain(a, voiceId(v, "latch"), v % 2 == 0 ? 1.0f : 0.0f);
        setPlain(a, voiceId(v, "gain"), -3.5f - 4.0f * static_cast<float>(v));
        setPlain(a, voiceId(v, "pitch"), 50.25f + 3.0f * static_cast<float>(v));
        setPlain(a, voiceId(v, "tuning"), static_cast<float>(v % 3 + 1));
        setPlain(a, voiceId(v, "degree"), static_cast<float>(3 + 5 * v));
        setPlain(a, voiceId(v, "octave"), static_cast<float>(v % 7 - 3 == 0 ? 2 : v % 7 - 3));
    }
    setPlain(a, "root", 52.0f);
    for (int v = 0; v < ep::kNumVoices; ++v) setPlain(a, voiceId(v, "snap"), 0.1f * static_cast<float>(v));
    setPlain(a, "snap_hysteresis", 17.0f);
    setPlain(a, "choir1_chord", 2.0f);
    setPlain(a, "choir2_chord", 3.0f);
    setPlain(a, voiceId(5, "tuning"), 4.0f); // the appended Choir mode round-trips too
    // The test must not pass vacuously: the non-default values really are in place.
    for (const auto& id : allExpectedIds(ep::kNumVoices)) {
        INFO(id);
        if (id.find("_latch") != std::string::npos && raw(a, id) == 0.0f) continue; // odd voices stay off
        CHECK(raw(a, id) != raw(defaults, id));
    }

    const auto saved = saveState(a);
    REQUIRE(saved.getSize() > 0);

    EthernautaAudioProcessor b;
    b.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (const auto& id : allExpectedIds(ep::kNumVoices)) {
        INFO(id);
        CHECK(raw(b, id) == raw(a, id));
    }

    // Schema version survives: present in the serialised blob and on the restored tree.
    const auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(xml != nullptr);
    CHECK(xml->getIntAttribute("schemaVersion", -1) == ep::kStateSchemaVersion);
    CHECK(static_cast<int>(b.getValueTreeState().state.getProperty("schemaVersion", -1)) == ep::kStateSchemaVersion);

    // Saving the restored processor reproduces the same blob.
    const auto again = saveState(b);
    CHECK(again == saved);
}

TEST_CASE("Older and newer schema versions load without losing parameters", "[plugin][state]")
{
    EthernautaAudioProcessor a;
    setPlain(a, "v3_gain", -20.0f);
    setPlain(a, "v6_pitch", 80.0f);
    const auto saved = saveState(a);
    auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(xml != nullptr);

    for (const int version : {0, 99}) {
        xml->setAttribute("schemaVersion", version);
        juce::MemoryBlock blob;
        juce::AudioProcessor::copyXmlToBinary(*xml, blob);
        EthernautaAudioProcessor b;
        b.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        CHECK(raw(b, "v3_gain") == Catch::Approx(-20.0f));
        CHECK(raw(b, "v6_pitch") == Catch::Approx(80.0f));
        // The loaded tree is re-stamped with the current schema version.
        CHECK(static_cast<int>(b.getValueTreeState().state.getProperty("schemaVersion", -1))
              == ep::kStateSchemaVersion);
    }
}

TEST_CASE("State saved by schema 1 (before the tuning engine) loads with tuning defaults", "[plugin][state][migration]")
{
    // Build the XML exactly as schema 1 wrote it: version 1 and only the four per-voice parameters.
    juce::XmlElement xml("EthernautaState");
    xml.setAttribute("schemaVersion", 1);
    auto add = [&xml](const std::string& id, double value) {
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", juce::String(id));
        e->setAttribute("value", value);
    };
    for (int v = 0; v < ep::kNumVoices; ++v) {
        add(voiceId(v, "wave"), 2.0);
        add(voiceId(v, "latch"), v % 2 == 0 ? 1.0 : 0.0);
        add(voiceId(v, "gain"), -9.0 - v);
        add(voiceId(v, "pitch"), 40.0 + v);
    }
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary(xml, blob);

    // The target processor already has non-default tuning, which an old preset must reset.
    EthernautaAudioProcessor p;
    setPlain(p, "root", 60.0f);
    for (int v = 0; v < ep::kNumVoices; ++v) {
        setPlain(p, voiceId(v, "tuning"), 2.0f);
        setPlain(p, voiceId(v, "degree"), 10.0f);
        setPlain(p, voiceId(v, "octave"), 2.0f);
    }
    p.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

    EthernautaAudioProcessor defaults;
    for (const auto& id : expectedTuningIds(ep::kNumVoices)) {
        INFO(id);
        CHECK(raw(p, id) == raw(defaults, id));
    }
    for (int v = 0; v < ep::kNumVoices; ++v) {
        INFO("voice " << v + 1);
        CHECK(raw(p, voiceId(v, "wave")) == 2.0f);
        CHECK(raw(p, voiceId(v, "latch")) == (v % 2 == 0 ? 1.0f : 0.0f));
        CHECK(raw(p, voiceId(v, "gain")) == Catch::Approx(-9.0f - static_cast<float>(v)));
        CHECK(raw(p, voiceId(v, "pitch")) == Catch::Approx(40.0f + static_cast<float>(v)));
    }
    CHECK(static_cast<int>(p.getValueTreeState().state.getProperty("schemaVersion", -1)) == ep::kStateSchemaVersion);
    CHECK(ep::kStateSchemaVersion == 4);

    // Re-saving writes the new schema.
    juce::MemoryBlock again;
    p.getStateInformation(again);
    const auto saved = juce::AudioProcessor::getXmlFromBinary(again.getData(), static_cast<int>(again.getSize()));
    REQUIRE(saved != nullptr);
    CHECK(saved->getIntAttribute("schemaVersion", -1) == 4);
}

TEST_CASE("State saved before the snap controls (schema 2) loads with snap defaults", "[plugin][state][migration]")
{
    // Build the XML exactly as schema 2 wrote it: version 2, no snap parameters at all.
    juce::XmlElement xml("EthernautaState");
    xml.setAttribute("schemaVersion", 2);
    auto add = [&xml](const std::string& id, double value) {
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", juce::String(id));
        e->setAttribute("value", value);
    };
    for (int v = 0; v < ep::kNumVoices; ++v) {
        add(voiceId(v, "gain"), -9.0 - v);
        add(voiceId(v, "tuning"), 2.0);
    }
    add("root", 50.0);
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary(xml, blob);

    // A fresh instance (what a host creates) and one that already has non-default snap values.
    EthernautaAudioProcessor fresh;
    EthernautaAudioProcessor used;
    for (int v = 0; v < ep::kNumVoices; ++v) setPlain(used, voiceId(v, "snap"), 0.25f);
    setPlain(used, "snap_hysteresis", 20.0f);

    for (auto* p : {&fresh, &used}) {
        p->setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        for (int v = 0; v < ep::kNumVoices; ++v) {
            INFO("voice " << v + 1);
            CHECK(raw(*p, voiceId(v, "snap")) == Catch::Approx(1.0f));
            CHECK(raw(*p, voiceId(v, "gain")) == Catch::Approx(-9.0f - static_cast<float>(v)));
            CHECK(raw(*p, voiceId(v, "tuning")) == 2.0f);
        }
        CHECK(raw(*p, "snap_hysteresis") == Catch::Approx(3.0f));
        CHECK(raw(*p, "root") == Catch::Approx(50.0f));
    }
}

TEST_CASE("State saved before choir tuning (schema 3) loads with choir defaults", "[plugin][state][migration]")
{
    // Build the XML exactly as schema 3 wrote it: version 3, snap controls present, no choir chords.
    juce::XmlElement xml("EthernautaState");
    xml.setAttribute("schemaVersion", 3);
    auto add = [&xml](const std::string& id, double value) {
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", juce::String(id));
        e->setAttribute("value", value);
    };
    for (int v = 0; v < ep::kNumVoices; ++v) {
        add(voiceId(v, "gain"), -9.0 - v);
        add(voiceId(v, "tuning"), 3.0); // Degree: an old index that must keep its meaning
        add(voiceId(v, "snap"), 0.5);
    }
    add("root", 50.0);
    add("snap_hysteresis", 12.0);
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary(xml, blob);

    EthernautaAudioProcessor fresh;
    EthernautaAudioProcessor used;
    setPlain(used, "choir1_chord", 3.0f);
    setPlain(used, "choir2_chord", 1.0f);

    for (auto* p : {&fresh, &used}) {
        p->setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
        CHECK(raw(*p, "choir1_chord") == 0.0f);
        CHECK(raw(*p, "choir2_chord") == 0.0f);
        for (int v = 0; v < ep::kNumVoices; ++v) {
            INFO("voice " << v + 1);
            CHECK(raw(*p, voiceId(v, "tuning")) == 3.0f);
            CHECK(raw(*p, voiceId(v, "snap")) == Catch::Approx(0.5f));
            CHECK(raw(*p, voiceId(v, "gain")) == Catch::Approx(-9.0f - static_cast<float>(v)));
        }
        CHECK(raw(*p, "snap_hysteresis") == Catch::Approx(12.0f));
        CHECK(raw(*p, "root") == Catch::Approx(50.0f));
        CHECK(static_cast<int>(p->getValueTreeState().state.getProperty("schemaVersion", -1)) == 4);
    }
}

TEST_CASE("Empty, null and malformed state leaves defaults untouched", "[plugin][state]")
{
    EthernautaAudioProcessor p;

    p.setStateInformation(nullptr, 0);
    p.setStateInformation(nullptr, 16);
    const char byte = 'x';
    p.setStateInformation(&byte, 0);
    p.setStateInformation(&byte, -5);
    expectAllDefaults(p);

    unsigned char garbage[64];
    for (std::size_t i = 0; i < sizeof(garbage); ++i) garbage[i] = static_cast<unsigned char>(i * 37u + 11u);
    p.setStateInformation(garbage, static_cast<int>(sizeof(garbage)));
    expectAllDefaults(p);

    // Truncated valid blob.
    EthernautaAudioProcessor src;
    setPlain(src, "v1_gain", -30.0f);
    const auto saved = saveState(src);
    p.setStateInformation(saved.getData(), static_cast<int>(saved.getSize() / 2));
    expectAllDefaults(p);
}

TEST_CASE("Foreign XML state is ignored", "[plugin][state]")
{
    EthernautaAudioProcessor p;
    juce::XmlElement foreign("SomeOtherPlugin");
    foreign.setAttribute("schemaVersion", 1);
    auto* child = foreign.createNewChildElement("PARAM");
    child->setAttribute("id", "v1_gain");
    child->setAttribute("value", -40.0);
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary(foreign, blob);
    p.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
    expectAllDefaults(p);

    // Plain (non-binary-wrapped) XML text is not the plugin's format either.
    const juce::String text = "<EthernautaState/>";
    p.setStateInformation(text.toRawUTF8(), static_cast<int>(text.getNumBytesAsUTF8()));
    expectAllDefaults(p);
}

TEST_CASE("Right tag with unknown and out-of-range parameters is handled safely", "[plugin][state]")
{
    EthernautaAudioProcessor p;
    juce::XmlElement xml("EthernautaState");
    xml.setAttribute("schemaVersion", ep::kStateSchemaVersion);
    auto add = [&xml](const char* id, double value) {
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", id);
        e->setAttribute("value", value);
    };
    add("v1_gain", 1.0e9);
    add("v2_pitch", -1.0e9);
    add("v3_wave", 77.0);
    add("v99_gain", 3.0); // no such voice
    juce::MemoryBlock blob;
    juce::AudioProcessor::copyXmlToBinary(xml, blob);
    p.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));

    CHECK(raw(p, "v1_gain") <= 6.0f);
    CHECK(raw(p, "v2_pitch") >= ep::kPitchMinNote);
    CHECK(raw(p, "v3_wave") >= 0.0f);
    CHECK(raw(p, "v3_wave") <= static_cast<float>(ethernauta::kNumWaveforms - 1));
    CHECK(p.getParameters().size() == ep::kTotalParams);

    // And the processor still renders finite audio afterwards.
    prepare(p, 48000.0, 512);
    CHECK(allFinite(render(p, 4096, 512).left));
}
