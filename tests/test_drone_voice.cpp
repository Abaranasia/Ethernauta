#include <ethernauta/DroneVoice.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

using ethernauta::DroneVoice;
using ethernauta::VoiceParams;
using ethernauta::Waveform;

namespace {
const Waveform kAll[] = {Waveform::Sine, Waveform::Saw, Waveform::Square, Waveform::Triangle};

VoiceParams loud(Waveform w, bool latched, float midiNote = 45.0f)
{
    VoiceParams p;
    p.waveform = w;
    p.latched = latched;
    p.gainDb = 0.0f;
    p.midiNote = midiNote;
    return p;
}

DroneVoice makeVoice(double sr, const VoiceParams& p)
{
    DroneVoice v;
    v.prepare(sr);
    v.setParams(p);
    return v;
}
} // namespace

TEST_CASE("VoiceParams defaults to A2 = 110 Hz, unlatched", "[voice]")
{
    const VoiceParams p;
    CHECK_FALSE(p.latched);
    CHECK(p.midiNote == 45.0f);
    CHECK(p.cents == 0.0f);
    CHECK(ethernauta::pitchToHz(p.midiNote, p.cents) == Catch::Approx(110.0).epsilon(1e-9));
    CHECK(ethernauta::pitchToHz(69.0f, 0.0f) == Catch::Approx(440.0).epsilon(1e-9));
    CHECK(ethernauta::pitchToHz(69.0f, 100.0f) == Catch::Approx(440.0 * std::pow(2.0, 1.0 / 12.0)).epsilon(1e-9));
}

TEST_CASE("Voice is silent while not latched", "[voice]")
{
    for (const double sr : testutil::kRates)
        for (const auto w : kAll) {
            auto v = makeVoice(sr, loud(w, false));
            const auto out = testutil::render(v, static_cast<int>(sr / 2), 512);
            REQUIRE(testutil::peak(out) == 0.0f);
            REQUIRE_FALSE(v.isActive());
        }
}

TEST_CASE("Idle voice leaves the output buffer untouched (additive contract)", "[voice]")
{
    auto v = makeVoice(48000.0, loud(Waveform::Saw, false));
    std::vector<float> buf(1000, 0.375f);
    v.process(buf.data(), static_cast<int>(buf.size()));
    for (const float s : buf) REQUIRE(s == 0.375f);
}

TEST_CASE("Voice process adds into the buffer instead of overwriting", "[voice]")
{
    auto v = makeVoice(48000.0, loud(Waveform::Sine, true));
    const auto reference = testutil::render(v, 4800, 4800);

    auto v2 = makeVoice(48000.0, loud(Waveform::Sine, true));
    std::vector<float> buf(4800, 0.25f);
    v2.process(buf.data(), 4800);
    for (std::size_t i = 0; i < buf.size(); ++i) REQUIRE(buf[i] == 0.25f + reference[i]);
}

TEST_CASE("Voice sounds after latching", "[voice]")
{
    for (const double sr : testutil::kRates)
        for (const auto w : kAll) {
            auto v = makeVoice(sr, loud(w, true));
            const auto out = testutil::render(v, static_cast<int>(sr), 512);
            const auto from = static_cast<std::size_t>(sr / 2);
            REQUIRE(testutil::rms(out, from) > 0.3);
            REQUIRE(v.isActive());
        }
}

TEST_CASE("Voice release returns to exact silence", "[voice]")
{
    for (const double sr : testutil::kRates)
        for (const auto w : kAll) {
            auto v = makeVoice(sr, loud(w, true));
            testutil::render(v, static_cast<int>(sr / 2), 512);
            auto p = loud(w, false);
            v.setParams(p);
            const auto out = testutil::render(v, static_cast<int>(sr * 2), 512);
            const auto tail = static_cast<std::size_t>(sr * 1.5); // well past the release time
            REQUIRE(testutil::peak(out, tail) == 0.0f);
            REQUIRE_FALSE(v.isActive());
        }
}

TEST_CASE("Voice can be re-latched after release", "[voice]")
{
    auto v = makeVoice(48000.0, loud(Waveform::Sine, true));
    testutil::render(v, 24000, 512);
    v.setLatched(false);
    testutil::render(v, 96000, 512);
    v.setLatched(true);
    const auto out = testutil::render(v, 48000, 512);
    CHECK(testutil::rms(out, 24000) > 0.3);
}

TEST_CASE("Voice output is finite and bounded across extreme settings", "[voice]")
{
    for (const auto w : kAll)
        for (const float note : {0.0f, 12.0f, 69.0f, 127.0f, 200.0f, -20.0f})
            for (const float gainDb : {-200.0f, -60.0f, 0.0f, 6.0f, 12.0f, 100.0f}) {
                VoiceParams p = loud(w, true, note);
                p.gainDb = gainDb;
                p.cents = 150.0f;
                auto v = makeVoice(44100.0, p);
                const auto out = testutil::render(v, 22050, 333);
                REQUIRE(testutil::allFinite(out));
                // Gain is clamped to kMaxGainDb (+12 dB = x3.98); oscillator peak <= ~1.1.
                REQUIRE(testutil::peak(out) <= 4.5f);
            }
}

TEST_CASE("Voice pitch is accurate across sample rates and waveforms", "[voice]")
{
    const double sr = GENERATE(44100.0, 48000.0, 96000.0);
    const auto w = GENERATE(Waveform::Sine, Waveform::Saw, Waveform::Square, Waveform::Triangle);
    const float note = GENERATE(33.0f, 45.0f, 57.0f, 69.0f);
    auto v = makeVoice(sr, loud(w, true, note));
    const auto out = testutil::render(v, static_cast<int>(sr * 1.5), 512);
    const double expected = ethernauta::pitchToHz(note, 0.0f);
    CHECK(testutil::estimateFrequency(out, sr, static_cast<std::size_t>(sr / 2)) == Catch::Approx(expected).epsilon(0.001));
}

TEST_CASE("Voice pitch honours cents", "[voice]")
{
    const double sr = 48000.0;
    VoiceParams p = loud(Waveform::Sine, true, 45.0f);
    p.cents = 50.0f;
    auto v = makeVoice(sr, p);
    const auto out = testutil::render(v, static_cast<int>(sr * 1.5), 512);
    CHECK(testutil::estimateFrequency(out, sr, static_cast<std::size_t>(sr / 2))
          == Catch::Approx(ethernauta::pitchToHz(45.0f, 50.0f)).epsilon(0.001));
}

TEST_CASE("Gain in dB scales the output level", "[voice]")
{
    const double sr = 48000.0;
    VoiceParams p = loud(Waveform::Sine, true);
    auto v0 = makeVoice(sr, p);
    p.gainDb = -6.0206f; // x0.5
    auto v6 = makeVoice(sr, p);
    const auto a = testutil::render(v0, 48000, 512);
    const auto b = testutil::render(v6, 48000, 512);
    CHECK(testutil::rms(b, 24000) / testutil::rms(a, 24000) == Catch::Approx(0.5).epsilon(0.01));
}

TEST_CASE("Gain at the floor mutes the voice completely", "[voice]")
{
    VoiceParams p = loud(Waveform::Sine, true);
    p.gainDb = ethernauta::kMinGainDb;
    auto v = makeVoice(48000.0, p);
    const auto out = testutil::render(v, 48000, 512);
    CHECK(testutil::peak(out, 24000) == 0.0f);
}

TEST_CASE("Output does not depend on the block size", "[voice][determinism]")
{
    const int segment = 7 * 4096; // divisible by every tested block size
    for (const auto w : kAll) {
        std::array<std::vector<float>, 4> results;
        const int blocks[] = {1, 7, 512, 4096};
        for (std::size_t b = 0; b < 4; ++b) {
            auto v = makeVoice(48000.0, loud(w, true));
            auto out = testutil::render(v, segment, blocks[b]);
            v.setLatched(false); // parameter change on a block boundary common to all block sizes
            const auto tail = testutil::render(v, segment, blocks[b]);
            out.insert(out.end(), tail.begin(), tail.end());
            results[b] = std::move(out);
        }
        for (std::size_t b = 1; b < 4; ++b) {
            REQUIRE(results[b].size() == results[0].size());
            REQUIRE(std::memcmp(results[b].data(), results[0].data(), results[0].size() * sizeof(float)) == 0);
        }
        // Block-size independence must not be achieved by producing silence.
        REQUIRE(testutil::rms(results[0], 0) > 0.1);
    }
}

TEST_CASE("Each waveform renders distinct audio through the voice", "[voice]")
{
    std::array<std::vector<float>, 4> outs;
    for (std::size_t i = 0; i < 4; ++i) {
        auto v = makeVoice(48000.0, loud(kAll[i], true));
        outs[i] = testutil::render(v, 24000, 512);
    }
    for (std::size_t a = 0; a < 4; ++a)
        for (std::size_t b = a + 1; b < 4; ++b) {
            double diff = 0.0;
            for (std::size_t i = 12000; i < outs[a].size(); ++i)
                diff += std::fabs(static_cast<double>(outs[a][i]) - outs[b][i]);
            CHECK(diff / 12000.0 > 0.05);
        }
}

TEST_CASE("Latching and unlatching do not click", "[voice][clicks]")
{
    const double sr = 48000.0;
    for (const auto w : kAll) {
        auto v = makeVoice(sr, loud(w, true));
        auto on = testutil::render(v, 48000, 64); // latch-on transient followed by steady state
        const float steadyStep = testutil::maxStep(on, 24000);
        const float onStep = testutil::maxStep(on, 1);
        v.setLatched(false);
        const auto off = testutil::render(v, 48000, 64);
        const float offStep = testutil::maxStep(off, 1);
        // The envelope may not add a step larger than the waveform's own steepest edge.
        CHECK(onStep <= steadyStep * 1.05f + 0.01f);
        CHECK(offStep <= steadyStep * 1.05f + 0.01f);
        if (w == Waveform::Sine || w == Waveform::Triangle) {
            CHECK(onStep < 0.05f);
            CHECK(offStep < 0.05f);
        }
    }
}

TEST_CASE("Changing gain while sounding does not click", "[voice][clicks]")
{
    auto v = makeVoice(48000.0, loud(Waveform::Sine, true));
    testutil::render(v, 24000, 128);
    v.setGainDb(-40.0f);
    const auto out = testutil::render(v, 24000, 128);
    CHECK(testutil::maxStep(out) < 0.05f);
}

TEST_CASE("Changing waveform while sounding stays finite and bounded", "[voice]")
{
    auto v = makeVoice(48000.0, loud(Waveform::Sine, true));
    for (int i = 0; i < 40; ++i) {
        v.setWaveform(kAll[i % 4]);
        const auto out = testutil::render(v, 777, 777);
        REQUIRE(testutil::allFinite(out));
        REQUIRE(testutil::peak(out) <= 1.25f);
    }
}

TEST_CASE("Waveform switch while sounding does not click, for every pair", "[voice][clicks][crossfade]")
{
    const double sr = 48000.0;
    for (const auto from : kAll)
        for (const auto to : kAll) {
            if (from == to) continue;
            for (const float note : {45.0f, 69.0f}) {
                auto v = makeVoice(sr, loud(from, true, note));
                auto out = testutil::render(v, 24000, 64);
                const float steadyFrom = testutil::maxStep(out, 12000);
                const std::size_t switchAt = out.size();
                v.setWaveform(to);
                const auto after = testutil::render(v, 12000, 64);
                out.insert(out.end(), after.begin(), after.end());
                const float steadyTo = testutil::maxStep(out, switchAt + 6000);
                // The transition may not add a step beyond the steepest edge of either waveform.
                const float bound = std::max(steadyFrom, steadyTo) * 1.1f + 0.02f;
                INFO(ethernauta::waveformName(from) << " -> " << ethernauta::waveformName(to) << " note " << note);
                CHECK(testutil::maxStep(out, switchAt) <= bound);
                CHECK(testutil::peak(out) <= 1.25f);
            }
        }
}

TEST_CASE("Rapid waveform switching stays continuous and bounded", "[voice][clicks][crossfade]")
{
    float steepest = 0.0f; // steepest per-sample step any waveform has on its own
    for (const auto w : kAll) {
        auto ref = makeVoice(48000.0, loud(w, true));
        steepest = std::max(steepest, testutil::maxStep(testutil::render(ref, 24000, 64), 12000));
    }
    auto v = makeVoice(48000.0, loud(Waveform::Sine, true));
    auto out = testutil::render(v, 24000, 64);
    for (int i = 0; i < 400; ++i) { // far faster than the crossfade length
        v.setWaveform(kAll[(i * 3 + 1) % 4]);
        const auto chunk = testutil::render(v, 37, 37);
        out.insert(out.end(), chunk.begin(), chunk.end());
    }
    CHECK(testutil::allFinite(out));
    CHECK(testutil::peak(out) <= 1.25f);
    CHECK(testutil::maxStep(out, 24000) <= steepest * 1.1f + 0.02f);
}

TEST_CASE("Output does not depend on the block size across a waveform switch", "[voice][determinism][crossfade]")
{
    const int segment = 7 * 4096;
    for (const auto to : {Waveform::Saw, Waveform::Square, Waveform::Triangle}) {
        std::array<std::vector<float>, 4> results;
        const int blocks[] = {1, 7, 512, 4096};
        for (std::size_t b = 0; b < 4; ++b) {
            auto v = makeVoice(48000.0, loud(Waveform::Sine, true));
            auto out = testutil::render(v, segment, blocks[b]);
            v.setWaveform(to);
            const auto tail = testutil::render(v, segment, blocks[b]);
            out.insert(out.end(), tail.begin(), tail.end());
            results[b] = std::move(out);
        }
        for (std::size_t b = 1; b < 4; ++b)
            REQUIRE(std::memcmp(results[b].data(), results[0].data(), results[0].size() * sizeof(float)) == 0);
    }
}

TEST_CASE("Waveform switch while idle leaves no artifact", "[voice][crossfade]")
{
    for (const auto from : kAll)
        for (const auto to : kAll) {
            auto v = makeVoice(48000.0, loud(from, false));
            v.setWaveform(to);
            REQUIRE(testutil::peak(testutil::render(v, 4800, 64)) == 0.0f);
            v.setLatched(true);
            const auto got = testutil::render(v, 9600, 64);
            // Identical to a voice that always had the new waveform: no fade from the old one.
            auto ref = makeVoice(48000.0, loud(to, true));
            const auto want = testutil::render(ref, 9600, 64);
            REQUIRE(std::memcmp(got.data(), want.data(), got.size() * sizeof(float)) == 0);
        }
}

TEST_CASE("Gain changed while idle does not ramp on the next latch", "[voice][clicks]")
{
    for (const bool viaSetParams : {false, true}) {
        auto v = makeVoice(48000.0, loud(Waveform::Sine, false)); // 0 dB target
        v.setGainDb(-6.0f);                                       // changed while idle
        if (viaSetParams) {
            VoiceParams p = loud(Waveform::Sine, true);
            p.gainDb = -20.0f;
            v.setParams(p);
        } else {
            v.setGainDb(-20.0f);
            v.setLatched(true);
        }
        const auto got = testutil::render(v, 4800, 64);

        VoiceParams q = loud(Waveform::Sine, true);
        q.gainDb = -20.0f;
        DroneVoice ref; // settings stored before prepare(): the gain starts exactly at its target
        ref.setParams(q);
        ref.prepare(48000.0);
        const auto want = testutil::render(ref, 4800, 64);
        REQUIRE(std::memcmp(got.data(), want.data(), got.size() * sizeof(float)) == 0);
    }
}

TEST_CASE("Gain changed after full release does not ramp on re-latch", "[voice][clicks]")
{
    auto v = makeVoice(48000.0, loud(Waveform::Sine, true));
    testutil::render(v, 4800, 64);
    v.setLatched(false);
    testutil::render(v, 48000, 512); // fully released -> idle
    REQUIRE_FALSE(v.isActive());
    v.setGainDb(-30.0f);
    v.setLatched(true);
    const auto out = testutil::render(v, 480, 64);
    // Envelope attack alone governs the level: -30 dB is x0.0316, so the peak can never exceed it.
    CHECK(testutil::peak(out) <= 0.0317f * 1.1f);
}

TEST_CASE("Voices are independent: one voice's params never alter another's output", "[voice][independence]")
{
    auto renderVoice = [](const VoiceParams& p) {
        auto v = makeVoice(48000.0, p);
        return testutil::render(v, 9600, 128);
    };
    std::array<VoiceParams, 6> base;
    for (std::size_t i = 0; i < base.size(); ++i)
        base[i] = loud(kAll[i % 4], true, 40.0f + 4.0f * static_cast<float>(i));

    for (std::size_t k = 0; k < base.size(); ++k) {
        auto changed = base;
        changed[k].waveform = kAll[(static_cast<std::size_t>(changed[k].waveform) + 1) % 4];
        changed[k].gainDb = -20.0f;
        changed[k].midiNote += 7.0f;
        changed[k].latched = false;
        for (std::size_t j = 0; j < base.size(); ++j) {
            if (j == k) continue;
            const auto a = renderVoice(base[j]);
            // Same array of voices, mixed together, with voice k changed.
            std::array<DroneVoice, 6> voices;
            std::array<std::vector<float>, 6> bufs;
            for (std::size_t n = 0; n < 6; ++n) {
                voices[n].prepare(48000.0);
                voices[n].setParams(changed[n]);
                bufs[n].assign(9600, 0.0f);
                for (int off = 0; off < 9600; off += 128) voices[n].process(bufs[n].data() + off, 128);
            }
            REQUIRE(std::memcmp(a.data(), bufs[j].data(), a.size() * sizeof(float)) == 0);
        }
    }
}

TEST_CASE("Six voices are an array, mixing is a loop", "[voice]")
{
    std::array<DroneVoice, 6> voices;
    std::vector<float> mix(4800, 0.0f);
    for (std::size_t i = 0; i < voices.size(); ++i) {
        voices[i].prepare(48000.0);
        VoiceParams p = loud(kAll[i % 4], i % 2 == 0, 33.0f + static_cast<float>(i) * 3.0f);
        p.gainDb = -18.0f;
        voices[i].setParams(p);
    }
    for (auto& v : voices) v.process(mix.data(), static_cast<int>(mix.size()));
    CHECK(testutil::allFinite(mix));
    CHECK(testutil::peak(mix) > 0.0f);
}

TEST_CASE("Processing before prepare is finite and safe", "[voice]")
{
    DroneVoice v; // default-constructed, never prepared
    v.setLatched(true);
    std::vector<float> buf(256, 0.0f);
    v.process(buf.data(), 256);
    CHECK(testutil::allFinite(buf));
}
