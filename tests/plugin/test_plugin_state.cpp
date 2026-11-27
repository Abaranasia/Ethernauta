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
    for (const auto& id : expectedIds(ep::kNumVoices)) {
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
    }
    // The test must not pass vacuously: the non-default values really are in place.
    for (const auto& id : expectedIds(ep::kNumVoices)) {
        INFO(id);
        if (id.find("_latch") != std::string::npos && raw(a, id) == 0.0f) continue; // odd voices stay off
        CHECK(raw(a, id) != raw(defaults, id));
    }

    const auto saved = saveState(a);
    REQUIRE(saved.getSize() > 0);

    EthernautaAudioProcessor b;
    b.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (const auto& id : expectedIds(ep::kNumVoices)) {
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
    CHECK(p.getParameters().size() == ep::kNumVoices * 4);

    // And the processor still renders finite audio afterwards.
    prepare(p, 48000.0, 512);
    CHECK(allFinite(render(p, 4096, 512).left));
}
