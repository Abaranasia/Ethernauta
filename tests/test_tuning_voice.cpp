#include <ethernauta/DroneVoice.h>
#include <ethernauta/TuningEngine.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestUtils.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace et = ethernauta;
using Catch::Approx;

namespace {
et::DroneVoice makeSine(double sr, bool latched)
{
    et::DroneVoice v;
    v.prepare(sr);
    et::VoiceParams p;
    p.waveform = et::Waveform::Sine;
    p.gainDb = 0.0f;
    p.latched = latched;
    v.setParams(p);
    return v;
}

// Sounding voice that starts exactly at `hz`: the frequency is set while idle, so it does not glide.
et::DroneVoice makeSineAt(double sr, double hz)
{
    auto v = makeSine(sr, false);
    v.setFrequencyHz(hz);
    v.setLatched(true);
    return v;
}

bool bitEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

double centsBetween(double a, double b) { return 1200.0 * std::log2(a / b); }
} // namespace

TEST_CASE("Frequency changes while idle jump straight to the target", "[voice][glide]")
{
    auto v = makeSine(48000.0, false);
    v.setFrequencyHz(220.0);
    CHECK(v.frequencyHz() == 220.0);
    CHECK(v.targetFrequencyHz() == 220.0);
}

TEST_CASE("Frequency changes while sounding glide and then settle exactly", "[voice][glide]")
{
    auto v = makeSine(48000.0, true);
    v.setFrequencyHz(110.0);
    testutil::render(v, 4800, 64); // past the attack
    v.setFrequencyHz(440.0);
    CHECK(v.targetFrequencyHz() == 440.0);

    std::vector<float> scratch(1, 0.0f);
    v.process(scratch.data(), 1);
    const double first = v.frequencyHz();
    CHECK(first > 110.0);
    CHECK(first < 130.0); // starts near the old pitch, no jump

    double prev = first;
    for (int i = 0; i < 48000; ++i) {
        scratch[0] = 0.0f;
        v.process(scratch.data(), 1);
        REQUIRE(v.frequencyHz() >= prev); // monotonic
        prev = v.frequencyHz();
    }
    for (int i = 0; i < 48000 * 3; ++i) v.process(scratch.data(), 1);
    CHECK(v.frequencyHz() == 440.0);
}

TEST_CASE("Glide has a time constant of kGlideSeconds in the log-frequency domain", "[voice][glide]")
{
    constexpr double sr = 48000.0;
    auto v = makeSineAt(sr, 100.0);
    v.setFrequencyHz(200.0); // +1200 cents
    const int n = static_cast<int>(et::DroneVoice::kGlideSeconds * sr);
    testutil::render(v, n, 64);
    const double remaining = centsBetween(200.0, v.frequencyHz());
    CHECK(remaining == Approx(1200.0 / std::exp(1.0)).epsilon(0.02));
}

TEST_CASE("Glide output is bit-identical for every block size", "[voice][glide][determinism]")
{
    // Retunes land on the same sample positions (multiples of 4096); only the block split differs.
    constexpr int kSegment = 4096;
    const double targets[] = {110.0, 165.0, 220.0, 137.5, 330.0, 110.0};
    std::vector<std::vector<float>> results;
    for (const int block : {1, 7, 512, 4096}) {
        auto v = makeSine(48000.0, true);
        std::vector<float> all;
        for (const double hz : targets) {
            v.setFrequencyHz(hz);
            const auto seg = testutil::render(v, kSegment, block);
            all.insert(all.end(), seg.begin(), seg.end());
        }
        results.push_back(std::move(all));
    }
    REQUIRE(testutil::rms(results[0], 4800) > 0.1);
    for (std::size_t i = 1; i < results.size(); ++i) REQUIRE(bitEqual(results[i], results[0]));
}

TEST_CASE("A retune while sounding never steps faster than the new pitch allows", "[voice][glide][clicks]")
{
    constexpr double sr = 48000.0;
    auto v = makeSine(sr, true);
    v.setFrequencyHz(110.0);
    testutil::render(v, 9600, 256);
    v.setFrequencyHz(880.0);
    const auto out = testutil::render(v, 48000, 256);
    const double maxAllowed = 2.0 * 3.14159265358979 * 880.0 / sr * 1.02;
    CHECK(testutil::maxStep(out) <= static_cast<float>(maxAllowed));
    CHECK(testutil::allFinite(out));
}

TEST_CASE("setPitch routes through the same glide", "[voice][glide]")
{
    auto v = makeSine(48000.0, true);
    v.setPitch(45.0f);
    testutil::render(v, 4800, 64);
    v.setPitch(57.0f);
    CHECK(v.targetFrequencyHz() == Approx(220.0).epsilon(1e-9));
    CHECK(v.frequencyHz() == Approx(110.0).epsilon(1e-9)); // not yet moved
    testutil::render(v, 48000 * 4, 512);
    CHECK(v.frequencyHz() == Approx(220.0).epsilon(1e-9));
}

TEST_CASE("prepare() keeps the tuned frequency instead of reverting to the pitch", "[voice][glide]")
{
    et::DroneVoice v;
    v.prepare(48000.0);
    v.setFrequencyHz(333.0);
    v.prepare(96000.0);
    CHECK(v.frequencyHz() == 333.0);
    CHECK(v.targetFrequencyHz() == 333.0);
}

TEST_CASE("Non-finite or absurd frequencies stay safe", "[voice][glide]")
{
    auto v = makeSine(48000.0, true);
    for (const double hz : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                            -1.0, 0.0, 1e300}) {
        v.setFrequencyHz(hz);
        const auto out = testutil::render(v, 2048, 128);
        CHECK(testutil::allFinite(out));
        CHECK(std::isfinite(v.frequencyHz()));
        CHECK(v.frequencyHz() > 0.0);
    }
}

TEST_CASE("Rendered frequency matches the engine target in every mode", "[voice][tuning][audio]")
{
    constexpr double sr = 48000.0;
    et::TuningEngine engine;
    engine.setRoot(45.0); // 110 Hz
    const double root = engine.rootHz();

    struct Case {
        et::VoiceTuning tuning;
        double freeHz;
        double expected;
    };
    auto tuning = [](et::TuningMode m, int degree = 0, int octave = 0) {
        et::VoiceTuning t;
        t.mode = m;
        t.degree = degree;
        t.octave = octave;
        return t;
    };
    const Case cases[] = {
        {tuning(et::TuningMode::Free), 173.0, 173.0},
        {tuning(et::TuningMode::EqualTempered), root * std::exp2(7.0 / 12.0) * 1.003, root * std::exp2(7.0 / 12.0)},
        {tuning(et::TuningMode::Just), root * std::exp2(7.0 / 12.0), root * 1.5},
        {tuning(et::TuningMode::Just, 0, 1), root * std::exp2(4.0 / 12.0), root * 2.5},
        {tuning(et::TuningMode::Degree, 13, 0), 999.0, root * 1.75},
        {tuning(et::TuningMode::Degree, 17 + 4, 0), 999.0, root * 5.0}, // partial 5
    };
    int voiceIndex = 0;
    for (const auto& c : cases) {
        auto v = makeSineAt(sr, engine.targetHz(voiceIndex++, c.tuning, c.freeHz));
        const auto out = testutil::render(v, 48000, 512);
        const double hz = testutil::estimateFrequency(out, sr, 4800);
        INFO("mode " << static_cast<int>(c.tuning.mode) << " expected " << c.expected << " measured " << hz);
        CHECK(hz == Approx(c.expected).epsilon(1e-3));
    }
}

TEST_CASE("Rendered frequency follows a root change through the glide", "[voice][tuning][audio]")
{
    constexpr double sr = 48000.0;
    et::TuningEngine engine;
    et::VoiceTuning t;
    t.mode = et::TuningMode::Degree;
    t.degree = 10; // 3/2
    engine.setRoot(45.0);

    auto v = makeSineAt(sr, engine.targetHz(0, t, 0.0));
    testutil::render(v, 9600, 512);

    engine.setRoot(50.0);
    v.setFrequencyHz(engine.targetHz(0, t, 0.0));
    const auto settled = testutil::render(v, 48000 * 2, 512);
    const double hz = testutil::estimateFrequency(settled, sr, 48000);
    CHECK(hz == Approx(et::midiNoteToHz(50.0) * 1.5).epsilon(1e-3));
}
