#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "PluginTestUtils.h"

#include <cmath>
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

// ---- Tuning engine wiring -------------------------------------------------------------------

namespace {
enum Tuning { kFree = 0, kEqualTempered = 1, kJust = 2, kDegree = 3 };

struct StackEntry {
    int degree;
    int octave;
    double ratio; // expected frequency / root
};

// Degrees 1/1, 3/2, 2/1, 5/4 (+1 oct), 7/4, 9/8 (+2 oct): one entry per voice.
constexpr StackEntry kStack[] = {{0, 0, 1.0},  {10, 0, 1.5},  {16, 0, 2.0},
                                 {6, 1, 2.5},  {13, 0, 1.75}, {3, 2, 4.5}};
static_assert(sizeof(kStack) / sizeof(kStack[0]) == ep::kNumVoices, "one stack entry per voice");

void configureStack(EthernautaAudioProcessor& p)
{
    for (int v = 0; v < ep::kNumVoices; ++v) {
        setPlain(p, voiceId(v, "tuning"), static_cast<float>(kDegree));
        setPlain(p, voiceId(v, "degree"), static_cast<float>(kStack[v].degree));
        setPlain(p, voiceId(v, "octave"), static_cast<float>(kStack[v].octave));
    }
}

// Fundamental of voice `voice` alone (others stay unlatched), measured after the glide has settled.
double measureVoice(EthernautaAudioProcessor& p, int voice, double sr, int total = 48000)
{
    latch(p, voice);
    prepare(p, sr, 512);
    const auto out = render(p, total, 512);
    return estimateFrequencyHz(out.left, sr, 4800);
}
} // namespace

TEST_CASE("A Just-degree stack plays root * ratio on every voice", "[plugin][audio][tuning]")
{
    constexpr double sr = 48000.0;
    for (const float rootNote : {45.0f, 50.0f, 38.0f}) {
        std::vector<double> measured;
        for (int v = 0; v < ep::kNumVoices; ++v) {
            EthernautaAudioProcessor p;
            setPlain(p, "root", rootNote);
            configureStack(p);
            const double hz = measureVoice(p, v, sr);
            measured.push_back(hz);
            INFO("root " << rootNote << " voice " << v + 1 << " measured " << hz);
            CHECK(hz == Catch::Approx(midiNoteToHz(rootNote) * kStack[v].ratio).epsilon(1e-3));
        }
        // Coherence: pairwise ratios are the table ratios, whatever the root.
        for (int i = 0; i < ep::kNumVoices; ++i)
            for (int j = 0; j < ep::kNumVoices; ++j)
                CHECK(measured[static_cast<std::size_t>(i)] / measured[static_cast<std::size_t>(j)]
                      == Catch::Approx(kStack[i].ratio / kStack[j].ratio).epsilon(2e-3));
    }
}

TEST_CASE("Changing the root retunes every voice while sounding", "[plugin][audio][tuning]")
{
    constexpr double sr = 48000.0;
    std::vector<double> before, after;
    for (int v = 0; v < ep::kNumVoices; ++v) {
        EthernautaAudioProcessor p;
        configureStack(p);
        latch(p, v);
        prepare(p, sr, 512);
        const auto first = render(p, 24000, 512);
        before.push_back(estimateFrequencyHz(first.left, sr, 4800));

        setPlain(p, "root", 52.0f); // 45 -> 52: a 3/2-ish jump up, glided
        const auto second = render(p, 48000 * 2, 512);
        after.push_back(estimateFrequencyHz(second.left, sr, 48000));

        INFO("voice " << v + 1);
        CHECK(before.back() == Catch::Approx(midiNoteToHz(45.0) * kStack[v].ratio).epsilon(1e-3));
        CHECK(after.back() == Catch::Approx(midiNoteToHz(52.0) * kStack[v].ratio).epsilon(1e-3));
        CHECK(after.back() / before.back() == Catch::Approx(midiNoteToHz(52.0) / midiNoteToHz(45.0)).epsilon(2e-3));
    }
}

TEST_CASE("Just mode snaps the pitch parameter to the root's pure ratio", "[plugin][audio][tuning]")
{
    constexpr double sr = 48000.0;
    const double root = midiNoteToHz(45.0);
    for (int v = 0; v < ep::kNumVoices; ++v) {
        // Pitch 49 is a 12-TET major third above the root (45): Just snaps it to 5/4, 12-TET keeps it.
        auto measureMode = [&](Tuning mode) {
            EthernautaAudioProcessor p;
            setPlain(p, voiceId(v, "pitch"), 49.0f);
            setPlain(p, voiceId(v, "tuning"), static_cast<float>(mode));
            return measureVoice(p, v, sr);
        };
        const double just = measureMode(kJust);
        const double tet = measureMode(kEqualTempered);
        const double free = measureMode(kFree);
        INFO("voice " << v + 1);
        CHECK(just == Catch::Approx(root * 1.25).epsilon(1e-3));
        CHECK(tet == Catch::Approx(midiNoteToHz(49.0)).epsilon(1e-3));
        CHECK(free == Catch::Approx(midiNoteToHz(49.0)).epsilon(1e-3));
        CHECK(std::fabs(just - tet) > 1.0);
    }
}

TEST_CASE("Each voice's snap parameter pulls its Just pitch toward the grid", "[plugin][audio][tuning][snap]")
{
    constexpr double sr = 48000.0;
    const double root = midiNoteToHz(45.0);
    for (int v = 0; v < ep::kNumVoices; ++v) {
        // Pitch 49 is a 12-TET major third (400 cents): the pure 5/4 is 386 cents, so the two differ.
        auto measure = [&](float strength) {
            EthernautaAudioProcessor p;
            setPlain(p, voiceId(v, "pitch"), 49.0f);
            setPlain(p, voiceId(v, "tuning"), static_cast<float>(kJust));
            setPlain(p, voiceId(v, "snap"), strength);
            return measureVoice(p, v, sr);
        };
        const double off = measure(0.0f);
        const double half = measure(0.5f);
        const double full = measure(1.0f);
        INFO("voice " << v + 1 << " off " << off << " half " << half << " full " << full);
        CHECK(off == Catch::Approx(midiNoteToHz(49.0)).epsilon(1e-3));
        CHECK(full == Catch::Approx(root * 1.25).epsilon(1e-3));
        CHECK(half < off);
        CHECK(half > full);
    }
}

TEST_CASE("The default snap strength keeps the fully snapped Just behaviour", "[plugin][audio][tuning][snap]")
{
    constexpr double sr = 48000.0;
    EthernautaAudioProcessor p;
    setPlain(p, voiceId(0, "pitch"), 49.0f);
    setPlain(p, voiceId(0, "tuning"), static_cast<float>(kJust));
    CHECK(measureVoice(p, 0, sr) == Catch::Approx(midiNoteToHz(45.0) * 1.25).epsilon(1e-3));
}

TEST_CASE("Global snap hysteresis reaches every voice", "[plugin][audio][tuning][snap]")
{
    constexpr double sr = 48000.0;
    const double root = midiNoteToHz(45.0);
    for (int v = 0; v < ep::kNumVoices; ++v) {
        // Settle on 5/4 (386 cents), then move to 416 cents: 9/7 (435) is nearer by only ~11 cents.
        // Without hysteresis the voice moves to 9/7; with 20 cents it stays on 5/4.
        auto settled = [&](float hysteresisCents) {
            EthernautaAudioProcessor p;
            setPlain(p, voiceId(v, "pitch"), 49.0f);
            setPlain(p, voiceId(v, "tuning"), static_cast<float>(kJust));
            latch(p, v);
            prepare(p, sr, 512);
            render(p, 24000, 512);
            setPlain(p, "snap_hysteresis", hysteresisCents);
            setPlain(p, voiceId(v, "pitch"), 49.16f);
            const auto out = render(p, 48000, 512);
            return estimateFrequencyHz(out.left, sr, 24000);
        };
        const double none = settled(0.0f);
        const double wide = settled(20.0f);
        INFO("voice " << v + 1 << " none " << none << " wide " << wide);
        CHECK(none == Catch::Approx(root * 9.0 / 7.0).epsilon(1e-3));
        CHECK(wide == Catch::Approx(root * 1.25).epsilon(1e-3));
    }
}

TEST_CASE("Free mode ignores the root, degree and octave parameters", "[plugin][audio][tuning]")
{
    auto renderWith = [](bool disturb) {
        EthernautaAudioProcessor p;
        latch(p, 0);
        if (disturb) {
            setPlain(p, "root", 70.0f);
            setPlain(p, voiceId(0, "degree"), 20.0f);
            setPlain(p, voiceId(0, "octave"), 3.0f);
        }
        prepare(p, 48000.0, 256);
        return render(p, 9600, 256);
    };
    const auto baseline = renderWith(false);
    REQUIRE(rms(baseline.left, 4800) > 0.01);
    CHECK(bitEqual(renderWith(true).left, baseline.left));
}

TEST_CASE("Tuned output with a mid-render root change is identical for every block size", "[plugin][audio][tuning][determinism]")
{
    constexpr int kSegment = 4096 * 2;
    std::vector<std::vector<float>> results;
    for (const int block : kBlocks) {
        EthernautaAudioProcessor p;
        configureStack(p);
        for (int v = 0; v < ep::kNumVoices; ++v) latch(p, v);
        prepare(p, 48000.0, block);
        auto all = render(p, kSegment, block).left;
        setPlain(p, "root", 50.0f);
        const auto second = render(p, kSegment, block).left;
        all.insert(all.end(), second.begin(), second.end());
        results.push_back(std::move(all));
    }
    REQUIRE(rms(results[0], 0) > 0.01);
    for (std::size_t i = 1; i < results.size(); ++i) REQUIRE(bitEqual(results[i], results[0]));
}

// ---- Choir tuning wiring ------------------------------------------------------------------------

TEST_CASE("Choir mode plays a triad from the root in both groups, with each group's own chord",
          "[plugin][audio][tuning][choir]")
{
    constexpr double sr = 48000.0;
    constexpr int kChoirMode = 4;
    // Frequency ratios [chord][member] written out literally (Major, Minor, Septimal Minor, Supermajor).
    constexpr double kRatio[4][3] = {
        {1.0, 5.0 / 4.0, 1.5}, {1.0, 6.0 / 5.0, 1.5}, {1.0, 7.0 / 6.0, 1.5}, {1.0, 9.0 / 7.0, 1.5}};
    constexpr int kOctave[ep::kNumVoices] = {0, 0, 1, 0, 1, 0};
    const int chordPairs[][2] = {{0, 1}, {2, 3}, {3, 0}};

    for (const auto& pair : chordPairs) {
        for (int v = 0; v < ep::kNumVoices; ++v) {
            EthernautaAudioProcessor p;
            setPlain(p, "root", 45.0f);
            setPlain(p, ep::kChoir1ChordParamId, static_cast<float>(pair[0]));
            setPlain(p, ep::kChoir2ChordParamId, static_cast<float>(pair[1]));
            for (int w = 0; w < ep::kNumVoices; ++w) {
                setPlain(p, voiceId(w, "tuning"), static_cast<float>(kChoirMode));
                setPlain(p, voiceId(w, "octave"), static_cast<float>(kOctave[w]));
                setPlain(p, voiceId(w, "pitch"), 80.0f); // must be ignored in Choir mode
            }
            const int chord = pair[v / 3];
            const double expected = midiNoteToHz(45.0) * kRatio[chord][v % 3] * (kOctave[v] == 1 ? 2.0 : 1.0);
            const double hz = measureVoice(p, v, sr);
            INFO("chords " << pair[0] << "/" << pair[1] << " voice " << v + 1 << " measured " << hz);
            CHECK(hz == Catch::Approx(expected).epsilon(1e-3));
        }
    }
}

TEST_CASE("A choir chord does not affect a voice that is not in Choir mode", "[plugin][audio][tuning][choir]")
{
    EthernautaAudioProcessor p;
    setPlain(p, ep::kChoir1ChordParamId, 3.0f); // Supermajor, but voice 2 stays in Free mode
    const double hz = measureVoice(p, 1, 48000.0);
    CHECK(hz == Catch::Approx(midiNoteToHz(static_cast<double>(ep::defaultPitchNote(1)))).epsilon(1e-3));
}

// ---- Choir drift wiring ---------------------------------------------------------------------------

TEST_CASE("Choir drift changes the audio in Choir mode only", "[plugin][audio][tuning][choir][drift]")
{
    constexpr double sr = 48000.0;
    constexpr int total = 48000 * 4;
    for (int mode = 0; mode < 5; ++mode) { // Free, 12-TET, Just, Degree, Choir
        auto renderWith = [&](float drift) {
            EthernautaAudioProcessor p;
            setPlain(p, "choir_drift", drift);
            setPlain(p, voiceId(1, "tuning"), static_cast<float>(mode));
            latch(p, 1);
            prepare(p, sr, 512);
            return render(p, total, 512).left;
        };
        const auto off = renderWith(0.0f);
        const auto on = renderWith(6.0f);
        REQUIRE(rms(off, 4800) > 0.01);
        INFO("tuning mode " << mode);
        if (mode == 4) {
            CHECK_FALSE(bitEqual(on, off));
            // Audible in the waveform, not a rounding-level difference: the drift slips the phase of a
            // 165 Hz tone by a visible fraction of a cycle within seconds.
            float maxDiff = 0.0f;
            for (std::size_t i = 0; i < on.size(); ++i) maxDiff = std::max(maxDiff, std::fabs(on[i] - off[i]));
            CHECK(maxDiff > 0.05f);
        } else {
            CHECK(bitEqual(on, off));
        }
    }
}

TEST_CASE("Choir drift is off by default and renders as before", "[plugin][audio][choir][drift]")
{
    constexpr double sr = 48000.0;
    auto renderChoir = [&](bool touchDrift) {
        EthernautaAudioProcessor p;
        if (touchDrift) setPlain(p, "choir_drift", 0.0f);
        for (int v = 0; v < ep::kNumVoices; ++v) {
            setPlain(p, voiceId(v, "tuning"), 4.0f);
            latch(p, v);
        }
        prepare(p, sr, 512);
        return render(p, 24000, 512).left;
    };
    CHECK(bitEqual(renderChoir(true), renderChoir(false)));
}

TEST_CASE("Drifting Choir output is identical for every block size", "[plugin][audio][determinism][choir][drift]")
{
    const int total = 7 * 4096;
    std::vector<Rendered> results;
    for (const int block : kBlocks) {
        EthernautaAudioProcessor p;
        setPlain(p, "choir_drift", 6.0f);
        for (int v = 0; v < ep::kNumVoices; ++v) {
            setPlain(p, voiceId(v, "tuning"), 4.0f);
            latch(p, v);
        }
        prepare(p, 48000.0, block);
        results.push_back(render(p, total, block));
    }
    REQUIRE(rms(results[0].left, 0) > 0.01);
    for (std::size_t i = 1; i < results.size(); ++i) REQUIRE(bitEqual(results[i].left, results[0].left));
}

TEST_CASE("Leaving Choir mode switches the drift off for that voice", "[plugin][audio][choir][drift]")
{
    // Once the depth smoother has fully snapped to 0 the voice is a plain Free voice again, so its
    // measured frequency is exactly the pitch parameter's (A2 = 110 Hz) rather than a drifting one.
    // Measure only after the snap: the depth decays as 6 * exp(-t / tau) until it is below the
    // smoother's snap epsilon, i.e. t = tau * ln(6 / epsilon) (constants of DroneVoice / ParamSmoother).
    constexpr double sr = 48000.0;
    constexpr double tau = 0.25;      // drift depth smoothing time constant, seconds
    constexpr double snapEps = 1.0e-7; // ParamSmoother::kSnapEpsilon, cents
    const double snapSeconds = tau * std::log(6.0 / snapEps);          // about 4.5 s
    const int settleSamples = static_cast<int>((snapSeconds + 1.0) * sr); // plus 1 s margin
    EthernautaAudioProcessor p;
    setPlain(p, "choir_drift", 6.0f);
    setPlain(p, voiceId(0, "tuning"), 4.0f);
    latch(p, 0);
    prepare(p, sr, 512);
    render(p, 48000, 512);
    setPlain(p, voiceId(0, "tuning"), 0.0f); // Free
    render(p, settleSamples, 512);           // glide finished and depth fully snapped to 0
    const auto out = render(p, 48000, 512);
    // 1e-7 relative is 1.7e-4 cents: a residual drift of 0.001 cent (5.8e-7) would fail.
    CHECK(estimateFrequencyHz(out.left, sr) == Catch::Approx(midiNoteToHz(45.0)).epsilon(1e-7));
}
