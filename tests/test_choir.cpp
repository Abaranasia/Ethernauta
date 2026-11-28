#include <ethernauta/Choir.h>
#include <ethernauta/Tuning.h>
#include <ethernauta/TuningEngine.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

namespace et = ethernauta;
using Catch::Approx;

namespace {

// The chord table, written out literally (num/den) so it does not depend on the code under test.
struct Lit {
    int num, den;
};
constexpr Lit kExpected[4][3] = {
    {{1, 1}, {5, 4}, {3, 2}},  // Major
    {{1, 1}, {6, 5}, {3, 2}},  // Minor
    {{1, 1}, {7, 6}, {3, 2}},  // Septimal minor
    {{1, 1}, {9, 7}, {3, 2}},  // Supermajor
};

constexpr et::ChoirChord kChords[4] = {et::ChoirChord::Major, et::ChoirChord::Minor,
                                       et::ChoirChord::SeptimalMinor, et::ChoirChord::Supermajor};

double expectedRatio(int chord, int member)
{
    return static_cast<double>(kExpected[chord][member].num) / static_cast<double>(kExpected[chord][member].den);
}

et::VoiceTuning choir(et::ChoirChord chord, int octave = 0)
{
    et::VoiceTuning t;
    t.mode = et::TuningMode::Choir;
    t.choirChord = chord;
    t.octave = octave;
    return t;
}

} // namespace

TEST_CASE("Choir: frozen enum values and counts", "[choir]")
{
    STATIC_REQUIRE(et::kChoirSize == 3);
    STATIC_REQUIRE(et::kNumChoirChords == 4);
    STATIC_REQUIRE(static_cast<int>(et::ChoirChord::Major) == 0);
    STATIC_REQUIRE(static_cast<int>(et::ChoirChord::Minor) == 1);
    STATIC_REQUIRE(static_cast<int>(et::ChoirChord::SeptimalMinor) == 2);
    STATIC_REQUIRE(static_cast<int>(et::ChoirChord::Supermajor) == 3);
    // Choir is appended after the existing modes; their values never change.
    STATIC_REQUIRE(static_cast<int>(et::TuningMode::Free) == 0);
    STATIC_REQUIRE(static_cast<int>(et::TuningMode::EqualTempered) == 1);
    STATIC_REQUIRE(static_cast<int>(et::TuningMode::Just) == 2);
    STATIC_REQUIRE(static_cast<int>(et::TuningMode::Degree) == 3);
    STATIC_REQUIRE(static_cast<int>(et::TuningMode::Choir) == 4);
    STATIC_REQUIRE(et::kNumTuningModes == 5);
}

TEST_CASE("Choir: exact ratios per chord and member", "[choir]")
{
    for (int c = 0; c < 4; ++c)
        for (int m = 0; m < 3; ++m) {
            INFO("chord " << c << " member " << m);
            CHECK(et::choirRatio(kChords[c], m) == expectedRatio(c, m)); // exact: same num/den division
        }
}

TEST_CASE("Choir: cents match 1200*log2(ratio)", "[choir]")
{
    for (int c = 0; c < 4; ++c)
        for (int m = 0; m < 3; ++m) {
            INFO("chord " << c << " member " << m);
            const double want = 1200.0 * std::log2(expectedRatio(c, m));
            CHECK(et::choirCents(kChords[c], m) == Approx(want).margin(1e-9));
        }
    CHECK(et::choirCents(et::ChoirChord::Major, 1) == Approx(386.313713865).margin(1e-6));
    CHECK(et::choirCents(et::ChoirChord::Major, 0) == 0.0);
}

TEST_CASE("Choir: out-of-range chord or member falls back to a safe value", "[choir]")
{
    // Out-of-range chord -> Major; out-of-range member -> the root (1/1).
    const auto badChord = static_cast<et::ChoirChord>(99);
    const auto negChord = static_cast<et::ChoirChord>(-1);
    for (int m = 0; m < 3; ++m) {
        CHECK(et::choirRatio(badChord, m) == expectedRatio(0, m));
        CHECK(et::choirRatio(negChord, m) == expectedRatio(0, m));
    }
    for (const int badMember : {-1, 3, 99, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        for (int c = 0; c < 4; ++c) {
            INFO("member " << badMember);
            CHECK(et::choirRatio(kChords[c], badMember) == 1.0);
            CHECK(et::choirCents(kChords[c], badMember) == 0.0);
        }
    CHECK(std::isfinite(et::choirCents(badChord, 7)));
}

TEST_CASE("Choir mode: voices map to members 0,1,2,0,1,2", "[choir][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    const double root = e.rootHz();
    const int member[6] = {0, 1, 2, 0, 1, 2};
    for (int v = 0; v < 6; ++v) {
        INFO("voice " << v);
        CHECK(e.targetHz(v, choir(et::ChoirChord::Major), 12345.0)
              == Approx(root * expectedRatio(0, member[v])).epsilon(1e-12));
    }
}

TEST_CASE("Choir mode: every chord at several roots and octaves", "[choir][engine]")
{
    et::TuningEngine e;
    for (const double note : {24.0, 45.0, 52.0, 60.0, 81.0}) {
        e.setRoot(note);
        const double root = 440.0 * std::exp2((note - 69.0) / 12.0);
        for (int c = 0; c < 4; ++c)
            for (int oct = -3; oct <= 3; ++oct)
                for (int v = 0; v < 6; ++v) {
                    INFO("root " << note << " chord " << c << " octave " << oct << " voice " << v);
                    const double want = root * expectedRatio(c, v % 3) * std::exp2(static_cast<double>(oct));
                    CHECK(e.targetHz(v, choir(kChords[c], oct), 777.0) == Approx(want).epsilon(1e-12));
                }
    }
}

TEST_CASE("Choir mode: octave is clamped like the other modes", "[choir][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    const double root = e.rootHz();
    CHECK(e.targetHz(0, choir(et::ChoirChord::Major, 99), 100.0) == Approx(root * 8.0).epsilon(1e-12));
    CHECK(e.targetHz(0, choir(et::ChoirChord::Major, -99), 100.0) == Approx(root / 8.0).epsilon(1e-12));
}

TEST_CASE("Choir mode ignores the voice's own pitch", "[choir][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    const double a = e.targetHz(1, choir(et::ChoirChord::Minor), 55.0);
    const double b = e.targetHz(1, choir(et::ChoirChord::Minor), 3000.0);
    const double c = e.targetHz(1, choir(et::ChoirChord::Minor), std::numeric_limits<double>::quiet_NaN());
    CHECK(a == b);
    CHECK(a == c);
    CHECK(a == Approx(e.rootHz() * 6.0 / 5.0).epsilon(1e-12));
}

TEST_CASE("Choir mode: a root change retunes the whole choir", "[choir][engine]")
{
    et::TuningEngine e;
    for (const double note : {45.0, 50.0, 40.0}) {
        e.setRoot(note);
        for (int v = 0; v < 3; ++v)
            CHECK(e.targetHz(v, choir(et::ChoirChord::Supermajor), 200.0)
                  == Approx(e.rootHz() * expectedRatio(3, v)).epsilon(1e-12));
    }
}

TEST_CASE("Choir mode: members form the right intervals against the shared root", "[choir][engine]")
{
    et::TuningEngine e;
    e.setRoot(57.0);
    for (int c = 0; c < 4; ++c) {
        const double r = e.targetHz(0, choir(kChords[c]), 1.0);
        const double third = e.targetHz(1, choir(kChords[c]), 1.0);
        const double fifth = e.targetHz(2, choir(kChords[c]), 1.0);
        INFO("chord " << c);
        CHECK(r == Approx(e.rootHz()).epsilon(1e-12));
        CHECK(third / r == Approx(static_cast<double>(kExpected[c][1].num) / kExpected[c][1].den).epsilon(1e-12));
        CHECK(fifth / r == Approx(1.5).epsilon(1e-12));
        // Third-to-fifth interval follows from the same root, with no accumulated drift.
        CHECK(fifth / third
              == Approx(1.5 / (static_cast<double>(kExpected[c][1].num) / kExpected[c][1].den)).epsilon(1e-12));
    }
    // Spot check of the literal intervals from the design.
    CHECK(e.targetHz(1, choir(et::ChoirChord::Major), 1.0) / e.rootHz() == Approx(1.25).epsilon(1e-12));
    CHECK(e.targetHz(1, choir(et::ChoirChord::Minor), 1.0) / e.rootHz() == Approx(1.2).epsilon(1e-12));
}

TEST_CASE("Choir mode: out-of-range voice, chord and member are safe", "[choir][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    const auto t = choir(et::ChoirChord::Major);
    CHECK(e.targetHz(-1, t, 321.0) == 321.0);
    CHECK(e.targetHz(et::TuningEngine::kMaxVoices, t, 321.0) == 321.0);
    CHECK(e.targetHz(-1, t, -5.0) > 0.0);

    auto bad = choir(static_cast<et::ChoirChord>(99));
    CHECK(e.targetHz(1, bad, 1.0) == Approx(e.rootHz() * 1.25).epsilon(1e-12));
    bad.choirChord = static_cast<et::ChoirChord>(-7);
    CHECK(e.targetHz(2, bad, 1.0) == Approx(e.rootHz() * 1.5).epsilon(1e-12));

    // Every voice slot up to kMaxVoices stays finite and positive, members wrapping by voice % 3.
    for (int v = 0; v < et::TuningEngine::kMaxVoices; ++v) {
        const double hz = e.targetHz(v, t, 100.0);
        CHECK(std::isfinite(hz));
        CHECK(hz > 0.0);
        CHECK(hz == Approx(e.rootHz() * expectedRatio(0, v % 3)).epsilon(1e-12));
    }
}

TEST_CASE("Choir mode does not disturb the other modes", "[choir][engine]")
{
    et::TuningEngine e;
    e.setRoot(45.0);
    const double root = e.rootHz();

    et::VoiceTuning degree;
    degree.mode = et::TuningMode::Degree;
    degree.degree = 10; // 3/2
    degree.choirChord = et::ChoirChord::Supermajor; // must be ignored outside Choir mode
    CHECK(e.targetHz(1, degree, 999.0) == Approx(root * 1.5).epsilon(1e-12));

    et::VoiceTuning just;
    just.mode = et::TuningMode::Just;
    just.choirChord = et::ChoirChord::Minor;
    // Alternate Choir and Just on the same voice: the snap memory must not leak across the switch.
    const double fifthAbove = root * et::centsToRatio(700.0);
    for (int i = 0; i < 3; ++i) {
        CHECK(e.targetHz(2, choir(et::ChoirChord::Major), 1.0) == Approx(root * 1.5).epsilon(1e-12));
        CHECK(e.targetHz(2, just, fifthAbove) == Approx(root * 1.5).epsilon(1e-12));
    }

    et::VoiceTuning freeT;
    CHECK(e.targetHz(0, freeT, 432.1) == 432.1);
    (void)e.targetHz(0, choir(et::ChoirChord::Major), 432.1);
    CHECK(e.targetHz(0, freeT, 432.1) == 432.1);
}
