#include <ethernauta/Tuning.h>
#include <ethernauta/TuningEngine.h>
#include <ethernauta/VoiceParams.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>

namespace et = ethernauta;
using Catch::Approx;

namespace {
double centsBetween(double a, double b) { return 1200.0 * std::log2(a / b); }

// The ratios of the research table (DRONE_RESEARCH.md 2.3), written out literally on purpose.
struct Lit {
    int num, den;
};
constexpr Lit kResearch[] = {{1, 1}, {16, 15}, {9, 8}, {10, 9}, {7, 6}, {6, 5}, {5, 4}, {9, 7}, {4, 3},
                             {7, 5}, {3, 2},   {8, 5}, {5, 3},  {7, 4}, {9, 5}, {15, 8}, {2, 1}};

et::VoiceTuning tuning(et::TuningMode m, int degree = 0, int octave = 0)
{
    et::VoiceTuning t;
    t.mode = m;
    t.degree = degree;
    t.octave = octave;
    return t;
}

// Free frequency that sits `cents` above the engine root.
double above(const et::TuningEngine& e, double cents) { return e.rootHz() * et::centsToRatio(cents); }
} // namespace

TEST_CASE("Tuning math: cents, ratio and semitones", "[tuning][math]")
{
    CHECK(et::ratioToCents(2.0) == Approx(1200.0).epsilon(1e-12));
    CHECK(et::centsToRatio(1200.0) == Approx(2.0).epsilon(1e-12));
    CHECK(et::ratioToCents(1.5) == Approx(701.955000865).epsilon(1e-9));
    CHECK(et::ratioToCents(1.0) == 0.0);
    CHECK(et::ratioToSemitones(2.0) == Approx(12.0).epsilon(1e-12));
    CHECK(et::semitonesToRatio(12.0) == Approx(2.0).epsilon(1e-12));
    CHECK(et::semitonesToRatio(-12.0) == Approx(0.5).epsilon(1e-12));
    for (const double r : {0.25, 0.8, 1.0, 1.25, 3.0, 17.0})
        CHECK(et::centsToRatio(et::ratioToCents(r)) == Approx(r).epsilon(1e-12));
}

TEST_CASE("midiNoteToHz agrees with pitchToHz", "[tuning][math]")
{
    CHECK(et::midiNoteToHz(69.0) == Approx(440.0).epsilon(1e-12));
    CHECK(et::midiNoteToHz(45.0) == Approx(110.0).epsilon(1e-12));
    for (const float note : {0.0f, 12.0f, 45.0f, 60.5f, 96.0f, 127.0f})
        CHECK(et::midiNoteToHz(note) == Approx(et::pitchToHz(note, 0.0f)).epsilon(1e-12));
}

TEST_CASE("Octave folding stays in [0, 1200) and [1, 2)", "[tuning][math]")
{
    CHECK(et::foldCents(0.0) == 0.0);
    CHECK(et::foldCents(1200.0) == 0.0);
    CHECK(et::foldCents(1201.5) == Approx(1.5).epsilon(1e-9));
    CHECK(et::foldCents(-100.0) == Approx(1100.0).epsilon(1e-12));
    CHECK(et::foldCents(-2400.0) == 0.0);
    for (const double c : {-1e-14, -1e-9, 1199.9999999999, 2399.999999999999, -3599.9999999999999}) {
        const double f = et::foldCents(c);
        INFO(c);
        CHECK(f >= 0.0);
        CHECK(f < 1200.0);
    }
    CHECK(et::foldRatio(1.0) == 1.0);
    CHECK(et::foldRatio(2.0) == 1.0);
    CHECK(et::foldRatio(3.0) == Approx(1.5).epsilon(1e-12));
    CHECK(et::foldRatio(0.75) == Approx(1.5).epsilon(1e-12));
    CHECK(et::foldRatio(5.0 / 16.0) == Approx(1.25).epsilon(1e-12));
}

TEST_CASE("Just table holds the research ratios with exact cents", "[tuning][table]")
{
    STATIC_REQUIRE(et::kNumScaleDegrees == 16);
    STATIC_REQUIRE(et::kNumJustDegrees == 33);

    // Scale part: sorted ascending, octave reduced, then the octave itself.
    for (int i = 0; i <= et::kNumScaleDegrees; ++i) {
        const auto& d = et::kJustTable[i];
        INFO(d.name);
        CHECK(std::fabs(d.cents - 1200.0 * std::log2(static_cast<double>(d.num) / d.den)) < 1e-6);
        CHECK(d.ratio() >= 1.0);
        CHECK(d.ratio() <= 2.0);
        if (i > 0) CHECK(d.cents > et::kJustTable[i - 1].cents);
    }
    CHECK(et::kJustTable[0].num == 1);
    CHECK(et::kJustTable[0].den == 1);
    CHECK(et::kJustTable[et::kNumScaleDegrees].num == 2);

    // Every research ratio is present, and nothing else is in the scale part.
    for (const auto& lit : kResearch) {
        bool found = false;
        for (int i = 0; i <= et::kNumScaleDegrees; ++i)
            found = found || (et::kJustTable[i].num == lit.num && et::kJustTable[i].den == lit.den);
        INFO(lit.num << "/" << lit.den);
        CHECK(found);
    }
    CHECK(et::kNumScaleDegrees + 1 == static_cast<int>(sizeof(kResearch) / sizeof(kResearch[0])));

    // Harmonic series partials 1..16 follow the scale part.
    for (int n = 1; n <= 16; ++n) {
        const auto& d = et::kJustTable[et::kPartialBase + n - 1];
        INFO("partial " << n);
        CHECK(d.num == n);
        CHECK(d.den == 1);
        CHECK(std::fabs(d.cents - 1200.0 * std::log2(static_cast<double>(n))) < 1e-6);
    }

    // Spot values of the research table (cents, 2 decimals).
    CHECK(et::kJustTable[6].cents == Approx(386.31).margin(0.005));  // 5/4
    CHECK(et::kJustTable[10].cents == Approx(701.96).margin(0.005)); // 3/2
    CHECK(et::kJustTable[13].cents == Approx(968.83).margin(0.005)); // 7/4
}

TEST_CASE("Free mode returns the voice's own frequency untouched", "[tuning][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    for (const double f : {27.5, 110.0, 163.7, 1234.5})
        CHECK(e.targetHz(0, tuning(et::TuningMode::Free, 5, 2), f) == f);
}

TEST_CASE("Root frequency follows pitchToHz conventions", "[tuning][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    CHECK(e.rootHz() == Approx(110.0).epsilon(1e-12));
    e.setRoot(57.0, 50.0);
    CHECK(e.rootHz() == Approx(et::pitchToHz(57.0f, 50.0f)).epsilon(1e-12));
}

TEST_CASE("12-TET snaps to the nearest semitone relative to the root", "[tuning][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.5, 12.0); // deliberately off the grid: the grid is relative to the root
    const auto t = tuning(et::TuningMode::EqualTempered);
    for (int n = -14; n <= 30; ++n) {
        const double want = e.rootHz() * std::exp2(n / 12.0);
        INFO("semitone " << n);
        CHECK(e.targetHz(0, t, above(e, 100.0 * n + 30.0)) == Approx(want).epsilon(1e-12));
        CHECK(e.targetHz(1, t, above(e, 100.0 * n - 30.0)) == Approx(want).epsilon(1e-12));
    }
}

TEST_CASE("Just mode snaps to the nearest table entry and keeps the octave", "[tuning][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    const auto t = tuning(et::TuningMode::Just);
    const double root = e.rootHz();
    // 12-TET fifth (700 c) -> 3/2; 12-TET major third (400 c) -> 5/4; minor seventh (1000 c) -> 9/5.
    CHECK(e.targetHz(0, t, above(e, 700.0)) == Approx(root * 3.0 / 2.0).epsilon(1e-12));
    CHECK(e.targetHz(0, t, above(e, 400.0)) == Approx(root * 5.0 / 4.0).epsilon(1e-12));
    CHECK(e.targetHz(0, t, above(e, 1000.0)) == Approx(root * 9.0 / 5.0).epsilon(1e-12));
    // One octave up and two octaves down keep the octave.
    CHECK(e.targetHz(0, t, above(e, 700.0 + 1200.0)) == Approx(root * 3.0).epsilon(1e-12));
    CHECK(e.targetHz(0, t, above(e, 700.0 - 2400.0)) == Approx(root * 3.0 / 8.0).epsilon(1e-12));
    // The octave parameter shifts on top of the snapped result.
    CHECK(e.targetHz(0, tuning(et::TuningMode::Just, 0, 2), above(e, 700.0)) == Approx(root * 6.0).epsilon(1e-12));
    CHECK(e.targetHz(0, tuning(et::TuningMode::Just, 0, -3), above(e, 700.0)) == Approx(root * 3.0 / 16.0).epsilon(1e-12));
}

TEST_CASE("Snapping folds correctly at the octave edges", "[tuning][engine][edge]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    const auto t = tuning(et::TuningMode::Just);
    const double root = e.rootHz();
    // Just below the octave: nearer to 2/1 than to 15/8.
    CHECK(e.targetHz(0, t, above(e, 1199.9)) == Approx(root * 2.0).epsilon(1e-12));
    CHECK(e.targetHz(0, t, above(e, 1190.0)) == Approx(root * 2.0).epsilon(1e-12));
    // Just below the root: still the root.
    CHECK(e.targetHz(1, t, above(e, -10.0)) == Approx(root).epsilon(1e-12));
    // Below the midpoint to 15/8 of the octave below (-55.87 c): snaps to 15/16 of the root.
    CHECK(e.targetHz(2, t, above(e, -60.0)) == Approx(root * 15.0 / 16.0).epsilon(1e-12));
    // Exactly on grid points, including exact multiples of the octave.
    CHECK(e.targetHz(3, t, root) == Approx(root).epsilon(1e-12));
    CHECK(e.targetHz(3, t, root * 2.0) == Approx(root * 2.0).epsilon(1e-12));
    CHECK(e.targetHz(3, t, root * 0.5) == Approx(root * 0.5).epsilon(1e-12));
}

TEST_CASE("Degree mode is root * ratio * 2^octave for every table entry", "[tuning][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    for (int d = 0; d < et::kNumJustDegrees; ++d)
        for (int oct = et::kMinOctaveOffset; oct <= et::kMaxOctaveOffset; ++oct) {
            const double want = e.rootHz() * et::kJustTable[d].num / et::kJustTable[d].den * std::exp2(oct);
            const double got = e.targetHz(0, tuning(et::TuningMode::Degree, d, oct), 12345.0);
            INFO("degree " << d << " octave " << oct);
            CHECK(std::fabs(centsBetween(got, want)) < 1e-6);
        }
    // Out-of-range degrees and octaves are clamped, never an out-of-bounds read.
    const double top = e.targetHz(0, tuning(et::TuningMode::Degree, 999, 99), 100.0);
    CHECK(top == Approx(e.rootHz() * 16.0 * 8.0).epsilon(1e-12));
    const double bottom = e.targetHz(0, tuning(et::TuningMode::Degree, -5, -99), 100.0);
    CHECK(bottom == Approx(e.rootHz() / 8.0).epsilon(1e-12));
}

TEST_CASE("A root change retunes every voice with exact ratios to the root", "[tuning][engine][coherence]")
{
    // A six-voice stack of Degree voices: 1/1, 3/2, 2/1, 5/4 (+1 oct), 7/4, 9/8 (+2 oct).
    const std::array<et::VoiceTuning, 6> stack = {
        tuning(et::TuningMode::Degree, 0, 0),  tuning(et::TuningMode::Degree, 10, 0),
        tuning(et::TuningMode::Degree, 16, 0), tuning(et::TuningMode::Degree, 6, 1),
        tuning(et::TuningMode::Degree, 13, 0), tuning(et::TuningMode::Degree, 3, 2)};
    const std::array<double, 6> ratio = {1.0, 3.0 / 2.0, 2.0, 5.0 / 2.0, 7.0 / 4.0, 9.0 / 2.0};

    et::TuningEngine e;
    for (const double note : {12.0, 33.3, 45.0, 60.0, 96.0, 127.0})
        for (const double cents : {-100.0, 0.0, 37.0, 100.0}) {
            e.setRoot(note, cents);
            std::array<double, 6> f{};
            for (std::size_t i = 0; i < f.size(); ++i) f[i] = e.targetHz(static_cast<int>(i), stack[i], 220.0);
            for (std::size_t i = 0; i < f.size(); ++i) {
                INFO("root " << note << " + " << cents << " voice " << i);
                CHECK(f[i] / e.rootHz() == Approx(ratio[i]).epsilon(1e-12));
                for (std::size_t j = 0; j < f.size(); ++j)
                    CHECK(f[i] / f[j] == Approx(ratio[i] / ratio[j]).epsilon(1e-12));
            }
        }
}

TEST_CASE("Snapped voices keep exact table ratios after a root change", "[tuning][engine][coherence]")
{
    const std::array<double, 6> freeHz = {110.0, 164.0, 220.0, 277.0, 330.0, 440.0};
    et::TuningEngine e;
    for (const auto mode : {et::TuningMode::Just, et::TuningMode::EqualTempered})
        for (const double note : {36.0, 45.0, 52.7, 71.0}) {
            e.setRoot(note);
            for (std::size_t i = 0; i < freeHz.size(); ++i) {
                const double f = e.targetHz(static_cast<int>(i), tuning(mode), freeHz[i]);
                const double c = et::foldCents(centsBetween(f, e.rootHz()));
                bool onGrid = false;
                if (mode == et::TuningMode::Just) {
                    for (int d = 0; d <= et::kNumScaleDegrees; ++d) // includes 2/1: fold may land on 1200
                        onGrid = onGrid || std::fabs(c - et::kJustTable[d].cents) < 1e-6;
                } else {
                    onGrid = std::fabs(c - 100.0 * std::round(c / 100.0)) < 1e-6;
                }
                INFO("mode " << static_cast<int>(mode) << " root " << note << " voice " << i << " cents " << c);
                CHECK(onGrid);
            }
        }
}

TEST_CASE("Snapping is idempotent", "[tuning][engine]")
{
    et::TuningEngine e;
    e.setRoot(47.3);
    for (const auto mode : {et::TuningMode::Just, et::TuningMode::EqualTempered})
        for (double f = 55.0; f < 3000.0; f *= 1.037) {
            const double once = e.targetHz(0, tuning(mode), f);
            const double twice = e.targetHz(0, tuning(mode), once);
            INFO("mode " << static_cast<int>(mode) << " input " << f);
            CHECK(twice == Approx(once).epsilon(1e-12));
        }
}

TEST_CASE("Hysteresis stops the target flapping around a midpoint", "[tuning][engine][hysteresis]")
{
    // Midpoint between 3/2 (701.955 c) and 8/5 (813.686 c).
    const double mid = 0.5 * (701.955000865 + 813.686286135);
    et::TuningEngine e;
    e.setRoot(45.0);
    const auto t = tuning(et::TuningMode::Just);
    const double fifth = e.rootHz() * 1.5;
    const double minorSixth = e.rootHz() * 1.6;

    REQUIRE(e.targetHz(0, t, above(e, mid - 20.0)) == Approx(fifth).epsilon(1e-12));
    // Jitter of +-0.5 cents around the midpoint never changes the decision made so far.
    int changes = 0;
    double last = fifth;
    for (int i = 0; i < 200; ++i) {
        const double f = e.targetHz(0, t, above(e, mid + ((i % 2) ? 0.5 : -0.5)));
        if (f != last) ++changes;
        last = f;
    }
    CHECK(changes == 0);
    // A clear move past the midpoint does switch ...
    CHECK(e.targetHz(0, t, above(e, mid + 5.0)) == Approx(minorSixth).epsilon(1e-12));
    // ... and the same jitter now sticks to the new target.
    changes = 0;
    last = minorSixth;
    for (int i = 0; i < 200; ++i) {
        const double f = e.targetHz(0, t, above(e, mid + ((i % 2) ? 0.5 : -0.5)));
        if (f != last) ++changes;
        last = f;
    }
    CHECK(changes == 0);
    // Moving back clearly flips it back.
    CHECK(e.targetHz(0, t, above(e, mid - 5.0)) == Approx(fifth).epsilon(1e-12));
}

TEST_CASE("Hysteresis memory is per voice and cleared on a mode change", "[tuning][engine][hysteresis]")
{
    const double mid = 0.5 * (701.955000865 + 813.686286135);
    et::TuningEngine e;
    e.setRoot(45.0);
    const auto t = tuning(et::TuningMode::Just);
    // Voice 0 settled below the midpoint, voice 1 above: same input, different (sticky) answers.
    e.targetHz(0, t, above(e, mid - 20.0));
    e.targetHz(1, t, above(e, mid + 20.0));
    const double f = above(e, mid + 0.5);
    CHECK(e.targetHz(0, t, f) == Approx(e.rootHz() * 1.5).epsilon(1e-12));
    CHECK(e.targetHz(1, t, f) == Approx(e.rootHz() * 1.6).epsilon(1e-12));
    // Leaving Just forgets the memory: re-entering decides from scratch (nearest wins).
    e.targetHz(0, tuning(et::TuningMode::Free), f);
    CHECK(e.targetHz(0, t, f) == Approx(e.rootHz() * 1.6).epsilon(1e-12));
}

TEST_CASE("Extreme roots and inputs never produce NaN or infinity", "[tuning][engine][extreme]")
{
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    et::TuningEngine e;
    const et::VoiceTuning modes[] = {tuning(et::TuningMode::Free), tuning(et::TuningMode::EqualTempered),
                                     tuning(et::TuningMode::Just), tuning(et::TuningMode::Degree, 32, 3),
                                     tuning(et::TuningMode::Degree, -1, -3)};
    for (const double note : {-1000.0, 0.0, 127.0, 1000.0, nan})
        for (const double cents : {-1000.0, -100.0, 100.0, 1000.0, nan}) {
            e.setRoot(note, cents);
            INFO("root " << note << " + " << cents);
            CHECK(std::isfinite(e.rootHz()));
            CHECK(e.rootHz() > 0.0);
            for (const auto& m : modes)
                for (const double f : {-5.0, 0.0, 1e-30, 1.0, 440.0, 1e30, inf, nan}) {
                    const double out = e.targetHz(0, m, f);
                    INFO("mode " << static_cast<int>(m.mode) << " input " << f);
                    CHECK(std::isfinite(out));
                    CHECK(out > 0.0);
                }
        }
    // Voice indices outside the engine's range fall back to the free frequency.
    CHECK(e.targetHz(-1, tuning(et::TuningMode::Just), 321.0) == 321.0);
    CHECK(e.targetHz(et::TuningEngine::kMaxVoices, tuning(et::TuningMode::Just), 321.0) == 321.0);
}
