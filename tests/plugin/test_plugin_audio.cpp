#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginTestUtils.h"

#include <cstring>

using namespace plugintest;
namespace ep = ethernauta::plugin;

namespace {
void latch(EthernautaAudioProcessor& p, int voiceIndex, bool on = true)
{
    setPlain(p, voiceId(voiceIndex, "latch"), on ? 1.0f : 0.0f);
}

bool bitEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}
} // namespace

TEST_CASE("No latched voice produces exact silence", "[plugin][audio]")
{
    for (const double sr : kRates)
        for (const int block : kBlocks) {
            EthernautaAudioProcessor p;
            prepare(p, sr, block);
            const auto out = render(p, 4096, block);
            INFO("sr " << sr << " block " << block);
            REQUIRE(peak(out.left) == 0.0f);
            REQUIRE(peak(out.right) == 0.0f);
        }
}

TEST_CASE("Latched voices produce audio on both channels", "[plugin][audio]")
{
    for (const double sr : kRates)
        for (const int block : kBlocks) {
            EthernautaAudioProcessor p;
            latch(p, 0);
            latch(p, 3);
            prepare(p, sr, block);
            const auto out = render(p, static_cast<int>(sr / 2), block);
            INFO("sr " << sr << " block " << block);
            REQUIRE(allFinite(out.left));
            REQUIRE(rms(out.left, 8192) > 0.01);
            REQUIRE(bitEqual(out.left, out.right));
            REQUIRE(peak(out.left) <= 1.0f);
        }
}

TEST_CASE("Every single voice is audible on its own", "[plugin][audio]")
{
    for (int v = 0; v < ep::kNumVoices; ++v) {
        EthernautaAudioProcessor p;
        latch(p, v);
        prepare(p, 48000.0, 512);
        const auto out = render(p, 24000, 512);
        INFO("voice " << v + 1);
        CHECK(rms(out.left, 12000) > 0.01);
    }
}

TEST_CASE("Output is identical for every block size", "[plugin][audio][determinism]")
{
    const int total = 7 * 4096; // divisible by every tested block size
    std::vector<Rendered> results;
    for (const int block : kBlocks) {
        EthernautaAudioProcessor p;
        for (int v = 0; v < ep::kNumVoices; ++v) {
            setPlain(p, voiceId(v, "wave"), static_cast<float>(v % 4));
            latch(p, v);
        }
        prepare(p, 48000.0, block);
        results.push_back(render(p, total, block));
    }
    REQUIRE(rms(results[0].left, 0) > 0.01);
    for (std::size_t i = 1; i < results.size(); ++i) REQUIRE(bitEqual(results[i].left, results[0].left));
}

TEST_CASE("Six latched voices at maximum gain never exceed full scale", "[plugin][audio][headroom]")
{
    for (const double sr : kRates) {
        EthernautaAudioProcessor p;
        for (int v = 0; v < ep::kNumVoices; ++v) {
            setPlain(p, voiceId(v, "wave"), v % 2 == 0 ? 1.0f : 2.0f); // saw / square: peakiest
            setPlain(p, voiceId(v, "gain"), 6.0f);                      // layout maximum
            setPlain(p, voiceId(v, "pitch"), 45.0f);                    // aligned phases
            latch(p, v);
        }
        prepare(p, sr, 512);
        const auto out = render(p, static_cast<int>(sr), 512);
        REQUIRE(allFinite(out.left));
        REQUIRE(peak(out.left) <= 1.0f);
        REQUIRE(rms(out.left, 8192) > 0.3); // loud, not silenced by the safeguard
    }
}

TEST_CASE("Unlatching every voice returns to exact silence", "[plugin][audio]")
{
    EthernautaAudioProcessor p;
    for (int v = 0; v < ep::kNumVoices; ++v) latch(p, v);
    prepare(p, 48000.0, 512);
    render(p, 24000, 512);
    for (int v = 0; v < ep::kNumVoices; ++v) latch(p, v, false);
    const auto out = render(p, 48000 * 2, 512);
    CHECK(peak(out.left, 48000 * 3 / 2) == 0.0f);
}

TEST_CASE("Voices are independent: other voices' settings do not change a voice's output", "[plugin][audio][independence]")
{
    auto renderWith = [](int changedVoice) {
        EthernautaAudioProcessor p;
        latch(p, 0);
        if (changedVoice > 0) { // change everything about another voice, but leave it unlatched
            setPlain(p, voiceId(changedVoice, "wave"), 2.0f);
            setPlain(p, voiceId(changedVoice, "gain"), 0.0f);
            setPlain(p, voiceId(changedVoice, "pitch"), 80.0f);
        }
        prepare(p, 48000.0, 256);
        return render(p, 9600, 256);
    };
    const auto baseline = renderWith(0);
    REQUIRE(rms(baseline.left, 4800) > 0.01);
    for (int k = 1; k < ep::kNumVoices; ++k) {
        INFO("changed voice " << k + 1);
        REQUIRE(bitEqual(renderWith(k).left, baseline.left));
    }
}

// Per-voice wiring: each test latches ONLY voice i and changes only voice i's parameter, so the
// rendered audio can only be right if voice i's own parameter drives voice i.

TEST_CASE("Each voice's own pitch parameter sets its frequency", "[plugin][audio][wiring]")
{
    constexpr double sr = 48000.0;
    for (int i = 0; i < ep::kNumVoices; ++i) {
        // Notes 40, 42, ... 50 differ from every default pitch (45, 52, 57, 61, 64, 69) and each other.
        const float note = 40.0f + 2.0f * static_cast<float>(i);
        EthernautaAudioProcessor p;
        setPlain(p, voiceId(i, "pitch"), note);
        latch(p, i);
        prepare(p, sr, 512);
        const auto out = render(p, static_cast<int>(sr), 512);
        const double hz = estimateFrequencyHz(out.left, sr, 4800);
        INFO("voice " << i + 1 << " measured " << hz << " Hz");
        // 0.5% << one semitone (5.9%): crossings are interpolated, so the estimate is much tighter.
        CHECK(hz == Catch::Approx(midiNoteToHz(note)).epsilon(0.005));
        for (int other = 0; other < ep::kNumVoices; ++other)
            CHECK(std::fabs(hz - midiNoteToHz(ep::defaultPitchNote(other))) > 1.0);
    }
}

TEST_CASE("Each voice's own gain parameter sets its level", "[plugin][audio][wiring]")
{
    constexpr double sr = 48000.0;
    constexpr float quietDb = -24.0f, loudDb = -12.0f;
    for (int i = 0; i < ep::kNumVoices; ++i) {
        auto levelAt = [&](float gainDb) {
            EthernautaAudioProcessor p;
            setPlain(p, voiceId(i, "gain"), gainDb);
            latch(p, i);
            prepare(p, sr, 512);
            return rms(render(p, static_cast<int>(sr), 512).left, 12000);
        };
        const double expected = std::pow(10.0, (loudDb - quietDb) / 20.0);
        const double ratio = levelAt(loudDb) / levelAt(quietDb);
        INFO("voice " << i + 1 << " ratio " << ratio << " expected " << expected);
        // 2%: the master tanh compresses the louder render by ~0.5% at its peak (see MasterStage.h);
        // both renders share pitch and window, so partial-cycle effects cancel in the ratio.
        CHECK(ratio == Catch::Approx(expected).epsilon(0.02));
    }
}

TEST_CASE("Each voice's own waveform parameter shapes its output", "[plugin][audio][wiring]")
{
    constexpr double sr = 48000.0;
    for (int i = 0; i < ep::kNumVoices; ++i) {
        auto crestFor = [&](float waveIndex) {
            EthernautaAudioProcessor p;
            setPlain(p, voiceId(i, "wave"), waveIndex);
            latch(p, i);
            prepare(p, sr, 512);
            return crestFactor(render(p, static_cast<int>(sr), 512).left, 12000);
        };
        const double sine = crestFor(0.0f);
        const double square = crestFor(2.0f);
        INFO("voice " << i + 1 << " crest sine " << sine << " square " << square);
        // Equal gain and pitch: only the shape differs. Sine ~1.41, square ~1.0 (ideal values).
        CHECK(sine == Catch::Approx(1.414).epsilon(0.05));
        CHECK(square < 1.15);
    }
}

TEST_CASE("Mono output buffers are supported", "[plugin][audio]")
{
    EthernautaAudioProcessor p;
    latch(p, 0);
    prepare(p, 48000.0, 512);
    juce::AudioBuffer<float> mono(1, 512);
    juce::MidiBuffer midi;
    float loudest = 0.0f;
    for (int i = 0; i < 40; ++i) {
        p.processBlock(mono, midi);
        loudest = std::max(loudest, mono.getMagnitude(0, 512));
    }
    CHECK(loudest > 0.01f);
}
