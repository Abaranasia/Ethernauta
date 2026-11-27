#include <ethernauta/Oscillator.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestUtils.h"

#include <cmath>
#include <vector>

using ethernauta::Oscillator;
using ethernauta::Waveform;

namespace {
std::vector<float> run(Waveform w, double hz, double sr, int n)
{
    Oscillator osc;
    osc.prepare(sr);
    osc.setWaveform(w);
    osc.setFrequency(hz);
    std::vector<float> out(static_cast<std::size_t>(n));
    for (auto& s : out) s = osc.process();
    return out;
}
const Waveform kAll[] = {Waveform::Sine, Waveform::Saw, Waveform::Square, Waveform::Triangle};
} // namespace

TEST_CASE("Oscillator frequency is accurate for every waveform and sample rate", "[oscillator]")
{
    const auto w = GENERATE(Waveform::Sine, Waveform::Saw, Waveform::Square, Waveform::Triangle);
    const double sr = GENERATE(44100.0, 48000.0, 96000.0);
    const double hz = GENERATE(55.0, 110.0, 1000.0);
    const auto out = run(w, hz, sr, static_cast<int>(sr));
    CHECK(testutil::estimateFrequency(out, sr) == Catch::Approx(hz).epsilon(0.001));
}

TEST_CASE("Oscillator output stays bounded and finite", "[oscillator]")
{
    for (const auto w : kAll)
        for (const double hz : {0.0, 20.0, 110.0, 5000.0, 20000.0, 30000.0}) {
            const auto out = run(w, hz, 48000.0, 48000);
            REQUIRE(testutil::allFinite(out));
            REQUIRE(testutil::peak(out) <= 1.25f);
        }
}

TEST_CASE("Oscillator waveforms are mutually distinct", "[oscillator]")
{
    std::vector<std::vector<float>> outs;
    for (const auto w : kAll) outs.push_back(run(w, 110.0, 48000.0, 4800));
    for (std::size_t a = 0; a < outs.size(); ++a)
        for (std::size_t b = a + 1; b < outs.size(); ++b) {
            double diff = 0.0;
            for (std::size_t i = 0; i < outs[a].size(); ++i)
                diff += std::fabs(static_cast<double>(outs[a][i]) - outs[b][i]);
            CHECK(diff / static_cast<double>(outs[a].size()) > 0.05);
        }
}

TEST_CASE("Oscillator sine matches the analytic sine", "[oscillator]")
{
    const double sr = 48000.0;
    const double hz = 100.0; // 480 samples per cycle
    const auto out = run(Waveform::Sine, hz, sr, 960);
    for (std::size_t i = 0; i < out.size(); ++i)
        REQUIRE(out[i] == Catch::Approx(std::sin(2.0 * 3.14159265358979323846 * hz * static_cast<double>(i) / sr)).margin(1e-4));
}

TEST_CASE("Oscillator PolyBLEP saw has far smaller edge jump than a naive saw", "[oscillator]")
{
    // A naive saw jumps by 2.0 at the wrap. The corrected saw spreads the edge over two samples.
    const auto out = run(Waveform::Saw, 1000.0, 48000.0, 4800);
    CHECK(testutil::maxStep(out) < 1.5f);
}

TEST_CASE("Oscillator reset restores the start phase", "[oscillator]")
{
    Oscillator osc;
    osc.prepare(48000.0);
    osc.setWaveform(Waveform::Saw);
    osc.setFrequency(220.0);
    const float first = osc.process();
    for (int i = 0; i < 1000; ++i) osc.process();
    osc.reset();
    CHECK(osc.process() == first);
}

TEST_CASE("Oscillator waveform names are stable", "[oscillator]")
{
    CHECK(std::string(ethernauta::waveformName(Waveform::Sine)) == "Sine");
    CHECK(std::string(ethernauta::waveformName(Waveform::Saw)) == "Saw");
    CHECK(std::string(ethernauta::waveformName(Waveform::Square)) == "Square");
    CHECK(std::string(ethernauta::waveformName(Waveform::Triangle)) == "Triangle");
}
