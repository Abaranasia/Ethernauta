#include <ethernauta/JustSnap.h>
#include <ethernauta/Tuning.h>
#include <ethernauta/TuningEngine.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace et = ethernauta;
using Catch::Approx;

namespace {
et::SnapCandidates justScale()
{
    et::SnapCandidates c;
    c.setJustScale();
    return c;
}

et::VoiceTuning justTuning(double strength, double hysteresis = et::kDefaultSnapHysteresisCents, int octave = 0)
{
    et::VoiceTuning t;
    t.mode = et::TuningMode::Just;
    t.octave = octave;
    t.snapStrength = strength;
    t.snapHysteresisCents = hysteresis;
    return t;
}

double above(const et::TuningEngine& e, double cents) { return e.rootHz() * et::centsToRatio(cents); }
double centsAbove(const et::TuningEngine& e, double hz) { return et::ratioToCents(hz / e.rootHz()); }
} // namespace

TEST_CASE("SnapCandidates: default just scale matches the sorted table", "[snap][candidates]")
{
    const auto c = justScale();
    REQUIRE(c.size() == et::kNumScaleDegrees);
    for (int i = 0; i < c.size(); ++i) {
        CHECK(c.cents(i) == et::kJustTable[i].cents);
        CHECK(c.ratio(i) == et::kJustTable[i].ratio());
    }
}

TEST_CASE("SnapCandidates: set() folds, sorts, dedups and drops non-finite values", "[snap][candidates]")
{
    et::SnapCandidates c;
    const double in[] = {702.0, -498.0 /* folds to 702 */, 1200.0 /* folds to 0 */, 386.0,
                         std::numeric_limits<double>::quiet_NaN(), 0.0, 2400.0 + 100.0};
    CHECK(c.set(in, 7) == 4);
    REQUIRE(c.size() == 4);
    CHECK(c.cents(0) == 0.0);
    CHECK(c.cents(1) == Approx(100.0));
    CHECK(c.cents(2) == Approx(386.0));
    CHECK(c.cents(3) == Approx(702.0));
    CHECK(c.ratio(1) == Approx(et::centsToRatio(100.0)).epsilon(1e-12));
}

TEST_CASE("SnapCandidates: capacity is fixed and extra values are ignored", "[snap][candidates]")
{
    double in[et::SnapCandidates::kMaxCandidates + 8];
    for (int i = 0; i < et::SnapCandidates::kMaxCandidates + 8; ++i) in[i] = 10.0 * i;
    et::SnapCandidates c;
    CHECK(c.set(in, et::SnapCandidates::kMaxCandidates + 8) == et::SnapCandidates::kMaxCandidates);
    CHECK(c.set(nullptr, 3) == 0);
    CHECK(c.set(in, -1) == 0);
    CHECK(c.size() == 0);
}

TEST_CASE("nearestKey picks the nearest candidate including the octave wraparound", "[snap][nearest]")
{
    const double in[] = {0.0, 400.0, 700.0}; // gap 700..1200 is 500 wide
    et::SnapCandidates c;
    c.set(in, 3);
    CHECK(c.keyCents(c.nearestKey(390.0)) == Approx(400.0));
    CHECK(c.keyCents(c.nearestKey(1150.0)) == Approx(1200.0)); // wraps up to T.front() + 1200
    CHECK(c.keyCents(c.nearestKey(-30.0)) == Approx(0.0).margin(1e-12));
    CHECK(c.keyCents(c.nearestKey(-450.0)) == Approx(-500.0)); // wraps down to T.back() - 1200
    CHECK(c.keyCents(c.nearestKey(2500.0)) == Approx(2400.0));
    CHECK(c.keyCents(c.nearestKey(-1200.0 + 395.0)) == Approx(-1200.0 + 400.0));
}

TEST_CASE("snapCents: strength 0 is identity, 1 is a full snap, in between is linear", "[snap][strength]")
{
    const auto c = justScale();
    et::SnapState s;
    const double cents = 395.0; // nearest is 5/4 at 386.3137
    const double target = et::kJustTable[6].cents;
    CHECK(et::snapCents(c, s, cents, 0.0, 0.0) == cents);
    et::SnapState s1;
    CHECK(et::snapCents(c, s1, cents, 1.0, 0.0) == Approx(target).epsilon(1e-12));
    for (const double k : {0.25, 0.5, 0.8}) {
        et::SnapState sk;
        CHECK(et::snapCents(c, sk, cents, k, 0.0) == Approx(cents + k * (target - cents)).epsilon(1e-12));
    }
}

TEST_CASE("snapCents: strength is clamped and NaN means no snap", "[snap][strength]")
{
    const auto c = justScale();
    et::SnapState a, b, d;
    const double cents = 395.0;
    CHECK(et::snapCents(c, a, cents, 7.0, 0.0) == Approx(et::kJustTable[6].cents).epsilon(1e-12));
    CHECK(et::snapCents(c, b, cents, -3.0, 0.0) == cents);
    CHECK(et::snapCents(c, d, cents, std::numeric_limits<double>::quiet_NaN(), 0.0) == cents);
}

TEST_CASE("snapCents: works across octaves and negative cents against the shared root", "[snap][nearest]")
{
    const auto c = justScale();
    for (const double oct : {-3.0, -1.0, 0.0, 2.0, 5.0}) {
        et::SnapState s;
        const double got = et::snapCents(c, s, 1200.0 * oct + 701.0, 1.0, 0.0);
        CHECK(got == Approx(1200.0 * oct + et::kJustTable[10].cents).epsilon(1e-12));
    }
    et::SnapState s;
    CHECK(et::snapCents(c, s, -1.0, 1.0, 0.0) == Approx(0.0).margin(1e-12));
    et::SnapState w;
    CHECK(et::snapCents(c, w, 1195.0, 1.0, 0.0) == Approx(1200.0).epsilon(1e-12)); // 1/1 of the next octave
}

TEST_CASE("snapCents: an empty candidate set leaves the pitch alone", "[snap][candidates]")
{
    et::SnapCandidates c;
    et::SnapState s;
    CHECK(et::snapCents(c, s, 123.4, 1.0, 0.0) == 123.4);
}

TEST_CASE("Hysteresis: no chattering at a midpoint, one switch once past the margin", "[snap][hysteresis]")
{
    const double in[] = {0.0, 400.0};
    et::SnapCandidates c;
    c.set(in, 2);
    constexpr double H = 5.0;
    et::SnapState s;

    // Settle on 0, then jitter around the midpoint (200) without ever beating 0 by more than H.
    CHECK(et::snapCents(c, s, 100.0, 1.0, H) == Approx(0.0).margin(1e-12));
    for (int i = 0; i < 100; ++i) {
        const double jitter = (i % 2 == 0) ? 201.0 : 199.0;
        CHECK(et::snapCents(c, s, jitter, 1.0, H) == Approx(0.0).margin(1e-12));
    }
    // Margin: the new point beats the old by more than H when |c-400| + H < |c-0|, i.e. c > 202.5.
    CHECK(et::snapCents(c, s, 202.4, 1.0, H) == Approx(0.0).margin(1e-12));
    CHECK(et::snapCents(c, s, 202.6, 1.0, H) == Approx(400.0));
    // Now it is sticky on 400 in the other direction.
    for (int i = 0; i < 100; ++i) {
        const double jitter = (i % 2 == 0) ? 199.0 : 201.0;
        CHECK(et::snapCents(c, s, jitter, 1.0, H) == Approx(400.0));
    }
    CHECK(et::snapCents(c, s, 197.4, 1.0, H) == Approx(0.0).margin(1e-12));
}

TEST_CASE("Hysteresis: zero or invalid hysteresis always follows the nearest candidate", "[snap][hysteresis]")
{
    const double in[] = {0.0, 400.0};
    et::SnapCandidates c;
    c.set(in, 2);
    for (const double h : {0.0, -4.0, std::numeric_limits<double>::quiet_NaN()}) {
        et::SnapState s;
        CHECK(et::snapCents(c, s, 190.0, 1.0, h) == Approx(0.0).margin(1e-12));
        CHECK(et::snapCents(c, s, 210.0, 1.0, h) == Approx(400.0));
        CHECK(et::snapCents(c, s, 190.0, 1.0, h) == Approx(0.0).margin(1e-12));
    }
}

TEST_CASE("Hysteresis state is independent per SnapState", "[snap][hysteresis]")
{
    const double in[] = {0.0, 400.0};
    et::SnapCandidates c;
    c.set(in, 2);
    et::SnapState a, b;
    et::snapCents(c, a, 50.0, 1.0, 10.0);
    et::snapCents(c, b, 350.0, 1.0, 10.0);
    CHECK(et::snapCents(c, a, 205.0, 1.0, 10.0) == Approx(0.0).margin(1e-12));
    CHECK(et::snapCents(c, b, 195.0, 1.0, 10.0) == Approx(400.0));
}

TEST_CASE("Hysteresis survives the octave wraparound without chattering", "[snap][hysteresis]")
{
    const double in[] = {0.0, 700.0}; // midpoint of 700 and 1200 is 950
    et::SnapCandidates c;
    c.set(in, 2);
    et::SnapState s;
    CHECK(et::snapCents(c, s, 900.0, 1.0, 5.0) == Approx(700.0));
    for (int i = 0; i < 50; ++i) CHECK(et::snapCents(c, s, (i % 2) ? 949.0 : 952.0, 1.0, 5.0) == Approx(700.0));
    CHECK(et::snapCents(c, s, 956.0, 1.0, 5.0) == Approx(1200.0));
}

namespace {
// Independent reference of the pre-snap Just behaviour (commit 9d6b9ad), written from first principles:
// an endless ladder of grid points (key = 16 * octave + degree over table rows 0..15), nearest point in cents
// against the shared root, hold rule "keep <= move + 3 cents", target = root * ratio * 2^(octave + offset).
// It deliberately uses neither snapCents, selectSnapKey nor TuningEngine.
struct RefJust {
    bool has = false;
    long long key = 0;

    static double keyCents(long long k)
    {
        long long oct = k / 16;
        long long idx = k - oct * 16;
        if (idx < 0) { idx += 16; --oct; }
        const auto& d = et::kJustTable[idx];
        return 1200.0 * static_cast<double>(oct) + 1200.0 * std::log2(static_cast<double>(d.num) / d.den);
    }

    double target(double rootHz, double freeHz, int octaveOffset)
    {
        const double cents = 1200.0 * std::log2(freeHz / rootHz);
        const long long base = static_cast<long long>(std::floor(cents / 1200.0)) * 16;
        long long best = base - 16;
        for (long long k = base - 16; k <= base + 32; ++k)
            if (std::fabs(cents - keyCents(k)) < std::fabs(cents - keyCents(best))) best = k;
        if (has && best != key && std::fabs(cents - keyCents(key)) <= std::fabs(cents - keyCents(best)) + 3.0)
            best = key;
        has = true;
        key = best;
        long long oct = best / 16;
        long long idx = best - oct * 16;
        if (idx < 0) { idx += 16; --oct; }
        const auto& d = et::kJustTable[idx];
        return rootHz * (static_cast<double>(d.num) / d.den) * std::exp2(static_cast<double>(oct + octaveOffset));
    }
};
} // namespace

TEST_CASE("Engine: Just mode strength 1 matches an independent reference of the pre-snap behaviour",
          "[snap][engine]")
{
    // Tolerance: the reference computes cents with log2 of the exact ratio while the engine uses cached 9-decimal
    // table cents, so grid decisions are identical away from exact ties (the sweep avoids them) and the
    // frequencies agree to a few ulps. Only the grid choice is asserted exactly (via the 1e-12 relative bound,
    // far below the ~1 cent spacing between distinct candidates); the Hz value is not claimed bit-exact.
    const double roots[] = {21.0, 33.0, 45.0, 57.3};
    const int octaves[] = {0, 1, -2, 3};
    int checked = 0;
    for (const double rootNote : roots)
        for (const int oct : octaves) {
            et::TuningEngine e;
            e.setRoot(rootNote);
            const double root = e.rootHz();
            et::VoiceTuning t = justTuning(1.0, et::kDefaultSnapHysteresisCents, oct);

            // (1) memoryless sweep: one fresh voice per probe (voice index cycles, memory reset in between).
            RefJust fresh;
            for (double c = -1250.0; c < 2500.0; c += 13.7) {
                e.reset();
                RefJust r;
                const double hz = root * std::exp2(c / 1200.0);
                CHECK(e.targetHz(0, t, hz) == Approx(r.target(root, hz, oct)).epsilon(1e-12));
                ++checked;
            }

            // (2) drift across every candidate midpoint (incl. the octave wrap) and back, 0.5 cent steps,
            // exercising the 3-cent hysteresis; also in other free-octaves. Offsets avoid exact ties (|keep-move| == 3),
            // where the engine's cached 9-decimal cents and the reference's exact log2 may legitimately differ.
            for (int k = 0; k < 16; ++k) {
                const double lo = RefJust::keyCents(k);
                const double hi = RefJust::keyCents(k + 1);
                const double mid = 0.5 * (lo + hi);
                for (const double shift : {0.0, 1200.0, -2400.0}) {
                    e.reset();
                    RefJust r;
                    std::vector<double> path;
                    for (int i = 0; i <= 32; ++i) path.push_back(mid - 8.1 + 0.5 * i); // never exactly +-1.5 or 0 from mid
                    for (int i = 32; i >= 0; --i) path.push_back(mid - 8.1 + 0.5 * i);
                    for (const double c : path) {
                        const double hz = root * std::exp2((c + shift) / 1200.0);
                        CHECK(e.targetHz(0, t, hz) == Approx(r.target(root, hz, oct)).epsilon(1e-12));
                        ++checked;
                    }
                }
            }
        }
    CHECK(checked > 5000);
}

TEST_CASE("Engine: strength 0 is the free frequency, bit for bit", "[snap][engine]")
{
    et::TuningEngine e;
    for (const double c : {5.0, 390.0, 1100.0}) {
        const double hz = above(e, c);
        CHECK(e.targetHz(2, justTuning(0.0), hz) == hz);
    }
}

TEST_CASE("Engine: partial strength pulls linearly in cents toward the snapped target", "[snap][engine]")
{
    et::TuningEngine e;
    const double cents = 395.0;
    const double target = et::kJustTable[6].cents;
    const double hz = e.targetHz(0, justTuning(0.5), above(e, cents));
    CHECK(centsAbove(e, hz) == Approx(cents + 0.5 * (target - cents)).margin(1e-9));
    // The octave offset is added to the result.
    et::TuningEngine e2;
    const double hz2 = e2.targetHz(0, justTuning(0.5, et::kDefaultSnapHysteresisCents, 1), above(e2, cents));
    CHECK(centsAbove(e2, hz2) == Approx(1200.0 + cents + 0.5 * (target - cents)).margin(1e-9));
}

TEST_CASE("Engine: snapped voices share one root and never chain", "[snap][engine]")
{
    et::TuningEngine e;
    e.setRoot(40.0);
    const double hz = e.targetHz(0, justTuning(1.0), above(e, 700.0));
    CHECK(hz == Approx(e.rootHz() * 1.5).epsilon(1e-12));
    e.setRoot(52.0);
    CHECK(e.targetHz(0, justTuning(1.0), above(e, 705.0)) == Approx(e.rootHz() * 1.5).epsilon(1e-9));
}

TEST_CASE("Engine: hysteresis is configurable per call and per voice", "[snap][engine][hysteresis]")
{
    et::TuningEngine e;
    // Between 10/9 (182.40, table[2]) and 9/8 (203.91, table[3]): the midpoint is 193.157.
    const double mid = 0.5 * (et::kJustTable[2].cents + et::kJustTable[3].cents);
    const double v0 = e.targetHz(0, justTuning(1.0, 20.0), above(e, mid - 1.0));
    const double v1 = e.targetHz(1, justTuning(1.0, 20.0), above(e, mid + 1.0));
    CHECK(centsAbove(e, v0) == Approx(et::kJustTable[2].cents).margin(1e-9));
    CHECK(centsAbove(e, v1) == Approx(et::kJustTable[3].cents).margin(1e-9));
    // Crossing the midpoint by 2 cents does not move a voice with a 20-cent margin...
    CHECK(e.targetHz(0, justTuning(1.0, 20.0), above(e, mid + 2.0)) == v0);
    CHECK(e.targetHz(1, justTuning(1.0, 20.0), above(e, mid - 2.0)) == v1);
    // ... but does with no hysteresis.
    CHECK(centsAbove(e, e.targetHz(0, justTuning(1.0, 0.0), above(e, mid + 2.0))) ==
          Approx(et::kJustTable[3].cents).margin(1e-9));
}

TEST_CASE("Engine: a custom candidate set drives the snap", "[snap][engine]")
{
    et::TuningEngine e;
    const double in[] = {0.0, 350.0, 700.0};
    et::SnapCandidates c;
    c.set(in, 3);
    e.setSnapCandidates(c);
    CHECK(centsAbove(e, e.targetHz(0, justTuning(1.0), above(e, 330.0))) == Approx(350.0).margin(1e-9));
    CHECK(centsAbove(e, e.targetHz(0, justTuning(1.0), above(e, 1100.0))) == Approx(1200.0).margin(1e-9));
}

TEST_CASE("Engine: snap output stays finite for extreme input and strength", "[snap][engine][extreme]")
{
    et::TuningEngine e;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const double s : {nan, -1.0, 0.5, 9.0, std::numeric_limits<double>::infinity()})
        for (const double hz : {nan, 0.0, -5.0, 1e-9, 1e12, 440.0}) {
            const double r = e.targetHz(0, justTuning(s, nan), hz);
            CHECK(std::isfinite(r));
            CHECK(r > 0.0);
        }
}

TEST_CASE("Engine: octave offset is applied continuously at every Just strength", "[snap][engine][octave]")
{
    const double cents = 395.0;
    double prev = 0.0;
    for (const double s : {0.0, 0.001, 1.0}) {
        et::TuningEngine e;
        const double hz = e.targetHz(0, justTuning(s, et::kDefaultSnapHysteresisCents, 1), above(e, cents));
        const double c = centsAbove(e, hz);
        CHECK(c > 1200.0 + 380.0); // octave present, never a whole-octave jump
        CHECK(c < 1200.0 + 410.0);
        if (s > 0.0) CHECK(std::fabs(c - prev) < 10.0);
        prev = c;
    }
    et::TuningEngine e;
    CHECK(e.targetHz(0, justTuning(0.0, et::kDefaultSnapHysteresisCents, 1), 200.0) == Approx(400.0).epsilon(1e-12));
}

TEST_CASE("Engine: an empty candidate set keeps the octave offset", "[snap][engine][octave]")
{
    et::TuningEngine e;
    e.setSnapCandidates(et::SnapCandidates{});
    CHECK(e.targetHz(0, justTuning(1.0, et::kDefaultSnapHysteresisCents, 1), 200.0) == Approx(400.0).epsilon(1e-12));
}

TEST_CASE("Engine: changing the candidate set drops stale snap memory", "[snap][engine][hysteresis]")
{
    et::TuningEngine e;
    // Voice 0 settles on key 0 (0 cents) of the default set.
    CHECK(centsAbove(e, e.targetHz(0, justTuning(1.0, 1000.0), above(e, 1.0))) == Approx(0.0).margin(1e-9));
    const double in[] = {0.0, 100.0, 200.0};
    et::SnapCandidates c;
    c.set(in, 3);
    e.setSnapCandidates(c);
    // 180 cents is nearest to 200; a stale key would hold 0 (its 50-cent margin is not exceeded).
    CHECK(centsAbove(e, e.targetHz(0, justTuning(1.0, 1000.0), above(e, 180.0))) == Approx(200.0).margin(1e-9));
}

TEST_CASE("Engine: 12-TET hold/switch uses a fixed 3-cent margin, Just uses snapHysteresisCents",
          "[snap][engine][hysteresis]")
{
    // 12-TET midpoint between semitone 0 and 1 is 50 cents. A held voice switches only once the input is
    // more than 3 cents past it (keep - move > 3  <=>  c > 51.5 going up, c < 48.5 coming back).
    const double seq[] = {49.0, 51.0, 51.4, 51.6, 49.0, 48.6, 48.4, 50.0, 52.0, 49.5};
    const int expectKey[] = {0, 0, 0, 1, 1, 1, 0, 0, 1, 1};

    auto run = [&](double hyst) {
        et::TuningEngine e;
        std::vector<double> out;
        et::VoiceTuning t;
        t.mode = et::TuningMode::EqualTempered;
        t.snapHysteresisCents = hyst;
        for (const double c : seq) out.push_back(e.targetHz(0, t, above(e, c)));
        return out;
    };
    et::TuningEngine ref;
    const auto base = run(et::kDefaultSnapHysteresisCents);
    for (size_t i = 0; i < base.size(); ++i)
        CHECK(centsAbove(ref, base[i]) == Approx(100.0 * expectKey[i]).margin(1e-9));
    // Bit-identical output whatever snapHysteresisCents says (0, 50, huge, NaN).
    for (const double h : {0.0, 50.0, 1e9, std::numeric_limits<double>::quiet_NaN()}) {
        const auto o = run(h);
        for (size_t i = 0; i < base.size(); ++i) CHECK(o[i] == base[i]);
    }

    // Just mode IS affected: the same kind of jitter across the 10/9 | 9/8 midpoint (193.157 cents).
    const double mid = 0.5 * (et::kJustTable[2].cents + et::kJustTable[3].cents);
    auto runJust = [&](double hyst) {
        et::TuningEngine e;
        double last = 0.0;
        e.targetHz(0, justTuning(1.0, hyst), above(e, mid - 1.0));
        last = e.targetHz(0, justTuning(1.0, hyst), above(e, mid + 5.0));
        return centsAbove(e, last);
    };
    CHECK(runJust(0.0) == Approx(et::kJustTable[3].cents).margin(1e-9));
    CHECK(runJust(50.0) == Approx(et::kJustTable[2].cents).margin(1e-9));
}
