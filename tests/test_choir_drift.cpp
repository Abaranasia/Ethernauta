#include <ethernauta/DriftLfo.h>
#include <ethernauta/DroneVoice.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

using ethernauta::DriftLfo;
using ethernauta::DroneVoice;
using ethernauta::VoiceParams;
using ethernauta::Waveform;

namespace {
constexpr double kPi = 3.14159265358979323846;

VoiceParams sineParams(float midiNote = 45.0f, bool latched = true)
{
    VoiceParams p;
    p.waveform = Waveform::Sine;
    p.latched = latched;
    p.gainDb = 0.0f;
    p.midiNote = midiNote;
    return p;
}

DroneVoice makeVoice(double sr, int voiceIndex, float depth, float midiNote = 45.0f)
{
    DroneVoice v;
    v.prepare(sr);
    v.setDriftVoiceIndex(voiceIndex);
    v.setDriftDepth(depth); // while idle: takes effect at once, no ramp
    v.setParams(sineParams(midiNote));
    return v;
}

// Current pitch offset from the tuned target in cents: first-principles 1200 * log2(f / target).
double offsetCents(const DroneVoice& v)
{
    return 1200.0 * std::log2(v.frequencyHz() / v.targetFrequencyHz());
}

// Offsets after each single-sample process() call.
std::vector<double> offsetSeries(DroneVoice& v, int samples)
{
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(samples));
    float sink = 0.0f;
    for (int i = 0; i < samples; ++i) {
        v.process(&sink, 1);
        out.push_back(offsetCents(v));
    }
    return out;
}

double maxAbs(const std::vector<double>& v)
{
    double m = 0.0;
    for (const double x : v) m = std::max(m, std::fabs(x));
    return m;
}

bool bitEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}
} // namespace

// ---- DriftLfo -----------------------------------------------------------------------------------

TEST_CASE("DriftLfo stays within [-1, 1] and is finite over a long run", "[drift][lfo]")
{
    constexpr double sr = 48000.0;
    for (int voice = 0; voice < 8; ++voice) {
        DriftLfo lfo;
        lfo.setVoiceIndex(voice);
        lfo.prepare(sr);
        double peak = 0.0;
        for (std::uint64_t n = 0; n < 48000ull * 1200ull; n += 997) { // 20 minutes, coarse grid
            const double x = lfo.at(n);
            REQUIRE(std::isfinite(x));
            REQUIRE(std::fabs(x) <= 1.0);
            peak = std::max(peak, std::fabs(x));
        }
        INFO("voice " << voice);
        CHECK(peak > 0.9); // two components: the sum comes close to the full +-1 over 20 minutes
    }
}

TEST_CASE("DriftLfo components are slow, incommensurate and differ per voice", "[drift][lfo]")
{
    std::vector<DriftLfo> lfos(6);
    for (int i = 0; i < 6; ++i) {
        lfos[static_cast<std::size_t>(i)].setVoiceIndex(i);
        lfos[static_cast<std::size_t>(i)].prepare(48000.0);
    }
    for (std::size_t i = 0; i < lfos.size(); ++i) {
        INFO("voice " << i);
        for (const double r : {lfos[i].rateAHz(), lfos[i].rateBHz()}) {
            CHECK(r >= 0.05);
            CHECK(r <= 0.2);
        }
        // The two components of a voice never share a rate (or an integer ratio below 1000 periods).
        CHECK(std::fabs(lfos[i].rateAHz() - lfos[i].rateBHz()) > 0.01);
        for (std::size_t j = i + 1; j < lfos.size(); ++j) {
            CHECK(lfos[i].rateAHz() != lfos[j].rateAHz());
            CHECK(lfos[i].rateBHz() != lfos[j].rateBHz());
            CHECK(lfos[i].phaseA() != lfos[j].phaseA());
        }
    }
}

TEST_CASE("DriftLfo value is the mean of two sines at its published rates and phases", "[drift][lfo]")
{
    constexpr double sr = 44100.0;
    DriftLfo lfo;
    lfo.setVoiceIndex(3);
    lfo.prepare(sr);
    for (const std::uint64_t n : {0ull, 1ull, 12345ull, 44100ull * 90ull}) {
        const double t = static_cast<double>(n) / sr;
        const double want = 0.5 * (std::sin(lfo.phaseA() + 2.0 * kPi * lfo.rateAHz() * t)
                                   + std::sin(lfo.phaseB() + 2.0 * kPi * lfo.rateBHz() * t));
        CHECK(lfo.at(n) == Catch::Approx(want).margin(1e-9));
    }
}

TEST_CASE("DriftLfo is deterministic and a pure function of the sample count", "[drift][lfo]")
{
    DriftLfo a;
    DriftLfo b;
    a.setVoiceIndex(2);
    b.setVoiceIndex(2);
    a.prepare(48000.0);
    b.prepare(48000.0);
    for (std::uint64_t n = 0; n < 100000; n += 313) REQUIRE(a.at(n) == b.at(n));
    // Not constant: it really moves.
    CHECK(std::fabs(a.at(0) - a.at(48000ull * 5ull)) > 0.01);
}

// ---- DroneVoice drift -----------------------------------------------------------------------------

TEST_CASE("Drift depth 0 renders bit-identical to a voice without drift", "[drift][voice]")
{
    for (const int block : {1, 64, 4096}) {
        DroneVoice plain;
        plain.prepare(48000.0);
        plain.setParams(sineParams());
        const auto want = testutil::render(plain, 48000, block);

        DroneVoice zero;
        zero.prepare(48000.0);
        zero.setDriftVoiceIndex(4);
        zero.setParams(sineParams());
        zero.setDriftDepth(0.0f);
        const auto got = testutil::render(zero, 48000, block);
        REQUIRE(testutil::rms(want) > 0.3);
        CHECK(bitEqual(got, want));
    }
}

TEST_CASE("Depth 0 stays bit-identical through a glide and a retune", "[drift][voice]")
{
    auto run = [](bool touchDrift) {
        DroneVoice v;
        v.prepare(48000.0);
        v.setParams(sineParams());
        if (touchDrift) v.setDriftDepth(0.0f);
        auto out = testutil::render(v, 4800, 100);
        v.setPitch(52.0f); // glide in flight
        const auto more = testutil::render(v, 9600, 100);
        out.insert(out.end(), more.begin(), more.end());
        return out;
    };
    CHECK(bitEqual(run(true), run(false)));
}

TEST_CASE("Drift offset never exceeds the depth", "[drift][voice]")
{
    for (const float depth : {1.0f, 3.0f, 6.0f})
        for (int voice = 0; voice < 6; ++voice) {
            auto v = makeVoice(48000.0, voice, depth);
            const auto offsets = offsetSeries(v, 48000 * 8);
            INFO("depth " << depth << " voice " << voice);
            CHECK(maxAbs(offsets) <= static_cast<double>(depth) + 1e-9);
            CHECK(maxAbs(offsets) > 0.05 * static_cast<double>(depth)); // and it is not stuck at 0
        }
}

TEST_CASE("Drift depth is clamped to the maximum and ignores non-finite values", "[drift][voice]")
{
    auto v = makeVoice(48000.0, 1, 100.0f);
    CHECK(v.driftDepth() == static_cast<float>(ethernauta::kMaxDriftCents));
    const auto offsets = offsetSeries(v, 48000 * 6);
    CHECK(maxAbs(offsets) <= ethernauta::kMaxDriftCents + 1e-9);
    CHECK(maxAbs(offsets) > 0.5);

    v.setDriftDepth(-3.0f);
    CHECK(v.driftDepth() == 0.0f);
    v.setDriftDepth(2.0f);
    v.setDriftDepth(std::nanf(""));
    CHECK(v.driftDepth() == 2.0f);
    v.setDriftDepth(INFINITY);
    CHECK(v.driftDepth() == 2.0f);
}

TEST_CASE("Different voices drift differently", "[drift][voice]")
{
    std::vector<std::vector<double>> series;
    for (int voice = 0; voice < 6; ++voice) {
        auto v = makeVoice(48000.0, voice, 6.0f);
        series.push_back(offsetSeries(v, 48000 * 10));
    }
    for (std::size_t a = 0; a < series.size(); ++a)
        for (std::size_t b = a + 1; b < series.size(); ++b) {
            double maxDiff = 0.0;
            for (std::size_t i = 0; i < series[a].size(); ++i)
                maxDiff = std::max(maxDiff, std::fabs(series[a][i] - series[b][i]));
            INFO("voices " << a << " and " << b);
            CHECK(maxDiff > 0.5); // cents
        }
}

TEST_CASE("Drift is deterministic: two runs render identical audio", "[drift][voice][determinism]")
{
    auto run = [] {
        auto v = makeVoice(48000.0, 2, 6.0f, 57.0f);
        return testutil::render(v, 48000 * 2, 333);
    };
    const auto a = run();
    const auto b = run();
    CHECK(bitEqual(a, b));
    // And the drift really changed the audio compared to depth 0.
    auto plain = makeVoice(48000.0, 2, 0.0f, 57.0f);
    CHECK_FALSE(bitEqual(a, testutil::render(plain, 48000 * 2, 333)));
}

TEST_CASE("Drifting output does not depend on the block size", "[drift][voice][determinism]")
{
    const int total = 7 * 4096 * 3; // divisible by every tested block size
    std::vector<std::vector<float>> results;
    for (const int block : {1, 7, 64, 512, 4096}) {
        auto v = makeVoice(48000.0, 5, 6.0f, 52.0f);
        auto out = testutil::render(v, total, block);
        v.setDriftDepth(2.5f); // parameter change on a boundary common to all block sizes
        const auto more = testutil::render(v, total, block);
        out.insert(out.end(), more.begin(), more.end());
        results.push_back(std::move(out));
    }
    REQUIRE(testutil::rms(results[0]) > 0.3);
    for (std::size_t i = 1; i < results.size(); ++i) {
        INFO("split " << i);
        CHECK(bitEqual(results[i], results[0]));
    }
    auto plain = makeVoice(48000.0, 5, 0.0f, 52.0f);
    CHECK_FALSE(bitEqual(std::vector<float>(results[0].begin(), results[0].begin() + total),
                         testutil::render(plain, total, 512)));
}

TEST_CASE("Rendered audio follows the drift offset", "[drift][voice]")
{
    // The mean pitch measured from the audio (zero crossings) in each window matches the mean offset
    // the voice reports, so the drift is really in the oscillator and not only in a getter.
    constexpr double sr = 48000.0;
    constexpr int window = 48000; // 1 s = 440 cycles
    constexpr int windows = 16;
    auto v = makeVoice(sr, 0, 6.0f, 69.0f); // 440 Hz
    double largest = 0.0;
    for (int w = 0; w < windows; ++w) {
        std::vector<float> audio(static_cast<std::size_t>(window), 0.0f);
        double sumOffset = 0.0;
        for (int i = 0; i < window; ++i) {
            v.process(&audio[static_cast<std::size_t>(i)], 1);
            sumOffset += offsetCents(v);
        }
        const double mean = sumOffset / window;
        const double measured = 1200.0 * std::log2(testutil::estimateFrequency(audio, sr) / 440.0);
        INFO("window " << w);
        CHECK(measured == Catch::Approx(mean).margin(0.1)); // cents
        largest = std::max(largest, std::fabs(mean));
    }
    CHECK(largest > 0.5); // a real offset was measured, not 0 == 0
}

TEST_CASE("Changing the depth does not click", "[drift][voice][clicks]")
{
    // Hard bound from the design, not from the code: the depth moves by at most 6 cents * coeff per
    // sample (tau 0.25 s -> 5e-4 cents at 48 kHz) and the drift itself by at most 6 * 0.5 * 2pi * (0.2 + 0.2)
    // cents/s = 7.6 cents/s = 1.6e-4 cents per sample. 2e-3 cents leaves a wide margin and is still
    // far below anything audible; an unsmoothed depth step would be up to 6 cents.
    constexpr double kMaxStep = 2e-3;
    auto v = makeVoice(48000.0, 0, 0.0f);
    std::vector<double> offsets = offsetSeries(v, 48000);
    v.setDriftDepth(6.0f);
    for (const double o : offsetSeries(v, 48000 * 3)) offsets.push_back(o);
    v.setDriftDepth(0.0f);
    for (const double o : offsetSeries(v, 48000 * 4)) offsets.push_back(o);
    double steepest = 0.0;
    for (std::size_t i = 1; i < offsets.size(); ++i) steepest = std::max(steepest, std::fabs(offsets[i] - offsets[i - 1]));
    CHECK(steepest < kMaxStep);
    // The ramp really happened: the drift was audible in the middle and gone at the end.
    CHECK(maxAbs(std::vector<double>(offsets.begin() + 48000, offsets.begin() + 4 * 48000)) > 0.5);
    CHECK(maxAbs(std::vector<double>(offsets.end() - 100, offsets.end())) < 0.05);
}

TEST_CASE("A depth change does not add a step to the audio", "[drift][voice][clicks]")
{
    auto v = makeVoice(48000.0, 1, 0.0f);
    testutil::render(v, 24000, 128);
    v.setDriftDepth(6.0f);
    const auto out = testutil::render(v, 96000, 128);
    // A 110 Hz sine at full scale steps at most 2*pi*110/48000 = 0.0144 per sample.
    CHECK(testutil::maxStep(out) < 0.0144f * 1.05f);
}

TEST_CASE("Drift reaches a meaningful fraction of its depth over a long run", "[drift][voice]")
{
    constexpr double sr = 8000.0; // low rate keeps the 20 minute run quick; the LFO rates are in Hz
    constexpr int block = 800;
    for (int voice = 0; voice < 6; ++voice) {
        auto v = makeVoice(sr, voice, 6.0f);
        std::vector<float> buf(block, 0.0f);
        double peak = 0.0;
        for (int b = 0; b < 1200 * 10; ++b) { // 1200 s sampled every 0.1 s
            v.process(buf.data(), block);
            peak = std::max(peak, std::fabs(offsetCents(v)));
        }
        INFO("voice " << voice << " peak " << peak);
        CHECK(peak >= 0.8 * 6.0);
        CHECK(peak <= 6.0 + 1e-9);
    }
}

TEST_CASE("Drift never changes the tuned target and an idle voice sits exactly at it", "[drift][voice]")
{
    auto v = makeVoice(48000.0, 3, 6.0f);
    const double target = v.targetFrequencyHz();
    testutil::render(v, 48000 * 3, 512);
    CHECK(v.targetFrequencyHz() == target);
    CHECK(std::fabs(offsetCents(v)) > 0.0);

    v.setLatched(false);
    testutil::render(v, 48000 * 2, 512); // past the release
    REQUIRE_FALSE(v.isActive());
    CHECK(v.frequencyHz() == v.targetFrequencyHz());
    CHECK(v.targetFrequencyHz() == target);

    // Idle: depth changes apply at once and still leave no offset; a relatch starts at the target.
    v.setDriftDepth(1.0f);
    CHECK(v.frequencyHz() == v.targetFrequencyHz());
    v.setLatched(true);
    CHECK(v.frequencyHz() == v.targetFrequencyHz());
    CHECK(v.driftDepth() == 1.0f);
}

TEST_CASE("Drift rides on a retune glide and settles within the depth of the new target", "[drift][voice]")
{
    auto v = makeVoice(48000.0, 4, 4.0f);
    testutil::render(v, 4800, 128);
    v.setPitch(57.0f);
    testutil::render(v, 48000 * 3, 128); // 37 time constants: the glide residue is far below 1e-9 cents
    CHECK(v.targetFrequencyHz() == Catch::Approx(ethernauta::pitchToHz(57.0f, 0.0f)).epsilon(1e-12));
    CHECK(std::fabs(offsetCents(v)) <= 4.0 + 1e-6);
}

TEST_CASE("reset restarts the drift clock", "[drift][voice]")
{
    auto v = makeVoice(48000.0, 2, 6.0f);
    const auto first = offsetSeries(v, 2000);
    testutil::render(v, 48000, 512);
    v.reset();
    v.setParams(sineParams());
    v.setDriftDepth(6.0f);
    const auto again = offsetSeries(v, 2000);
    REQUIRE(first.size() == again.size());
    for (std::size_t i = 0; i < first.size(); ++i) REQUIRE(first[i] == again[i]);
}
