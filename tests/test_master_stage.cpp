#include <ethernauta/DroneVoice.h>
#include <ethernauta/MasterStage.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "TestUtils.h"

#include <array>
#include <vector>

using ethernauta::DroneVoice;
using ethernauta::VoiceParams;
using ethernauta::Waveform;

TEST_CASE("Master stage maps silence to exact silence", "[master]")
{
    CHECK(ethernauta::masterStage(0.0f) == 0.0f);
    std::vector<float> buf(512, 0.0f);
    ethernauta::applyMasterStage(buf.data(), static_cast<int>(buf.size()));
    CHECK(testutil::peak(buf) == 0.0f);
}

TEST_CASE("Master stage is odd, monotonic, bounded and near-linear for small signals", "[master]")
{
    float prev = ethernauta::masterStage(-100.0f);
    for (float x = -100.0f; x <= 100.0f; x += 0.25f) {
        const float y = ethernauta::masterStage(x);
        CHECK(y >= prev);
        CHECK(std::fabs(y) <= 1.0f);
        CHECK(ethernauta::masterStage(-x) == -y);
        prev = y;
    }
    CHECK(ethernauta::masterStage(0.01f) == Catch::Approx(0.01f * ethernauta::kMasterTrim).epsilon(1e-3));
}

TEST_CASE("Six simultaneous latched voices at maximum gain stay within [-1, 1]", "[master][headroom]")
{
    const Waveform waves[] = {Waveform::Saw, Waveform::Square, Waveform::Saw, Waveform::Square, Waveform::Saw,
                              Waveform::Square};
    for (const double sr : testutil::kRates) {
        std::array<DroneVoice, 6> voices;
        std::vector<float> mix(static_cast<std::size_t>(sr), 0.0f);
        for (std::size_t i = 0; i < voices.size(); ++i) {
            voices[i].prepare(sr);
            VoiceParams p;
            p.waveform = waves[i];
            p.latched = true;
            p.gainDb = ethernauta::kMaxGainDb; // worst case: the core's loudest setting
            p.midiNote = 45.0f;                // identical pitch: phases line up, peaks add up
            voices[i].setParams(p);
        }
        for (auto& v : voices) v.process(mix.data(), static_cast<int>(mix.size()));
        const float rawPeak = testutil::peak(mix);
        REQUIRE(rawPeak > 6.0f); // the raw sum really does exceed full scale
        ethernauta::applyMasterStage(mix.data(), static_cast<int>(mix.size()));
        REQUIRE(testutil::allFinite(mix));
        REQUIRE(testutil::peak(mix) <= 1.0f);
    }
}

TEST_CASE("Master stage keeps a default-level voice nearly untouched", "[master]")
{
    DroneVoice v;
    v.prepare(48000.0);
    VoiceParams p; // default -12 dB sine
    p.latched = true;
    v.setParams(p);
    auto dry = testutil::render(v, 24000, 512);
    auto wet = dry;
    ethernauta::applyMasterStage(wet.data(), static_cast<int>(wet.size()));
    // tanh compresses most at the peak, so the relative deviation from the linear (trimmed) signal
    // is largest there. Documented in MasterStage.h: ~0.5% (x^2/3 with x = 0.251 * 0.5).
    constexpr std::size_t kSettled = 12000;
    std::size_t peakIdx = kSettled;
    for (std::size_t i = kSettled; i < dry.size(); ++i)
        if (std::fabs(dry[i]) > std::fabs(dry[peakIdx])) peakIdx = i;
    const double linear = static_cast<double>(dry[peakIdx]) * ethernauta::kMasterTrim;
    const double relDeviation = std::fabs(linear - static_cast<double>(wet[peakIdx])) / std::fabs(linear);
    INFO("peak " << dry[peakIdx] << " relative deviation " << relDeviation);
    CHECK(std::fabs(dry[peakIdx]) == Catch::Approx(0.2512).epsilon(0.01)); // -12 dB
    CHECK(relDeviation < 0.006); // measured 0.52%; the old "<0.2%" claim would fail here
    CHECK(relDeviation > 0.004); // two-sided so the documented ~0.5% cannot silently drift
}
