#include <ethernauta/LatchGate.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>

using ethernauta::LatchGate;

TEST_CASE("LatchGate starts closed at exactly zero", "[gate]")
{
    LatchGate g;
    g.prepare(48000.0, 0.01, 0.05);
    CHECK(g.isIdle());
    for (int i = 0; i < 100; ++i) CHECK(g.next() == 0.0f);
}

TEST_CASE("LatchGate attack reaches exactly 1 within the attack time", "[gate]")
{
    LatchGate g;
    g.prepare(48000.0, 0.01, 0.05); // 480 samples attack
    g.setOpen(true);
    float v = 0.0f;
    float prev = 0.0f;
    for (int i = 0; i < 481; ++i) {
        v = g.next();
        REQUIRE(v >= prev);
        prev = v;
    }
    CHECK(v == 1.0f);
    CHECK_FALSE(g.isIdle());
}

TEST_CASE("LatchGate release returns to exactly 0 and goes idle", "[gate]")
{
    LatchGate g;
    g.prepare(48000.0, 0.01, 0.05); // 2400 samples release
    g.setOpen(true);
    for (int i = 0; i < 1000; ++i) g.next();
    g.setOpen(false);
    float prev = 1.0f;
    for (int i = 0; i < 2401; ++i) {
        const float v = g.next();
        REQUIRE(v <= prev);
        prev = v;
    }
    CHECK(prev == 0.0f);
    CHECK(g.isIdle());
}

TEST_CASE("LatchGate reversing mid-ramp stays continuous", "[gate]")
{
    LatchGate g;
    g.prepare(48000.0, 0.01, 0.05);
    g.setOpen(true);
    float prev = 0.0f;
    float maxStep = 0.0f;
    for (int i = 0; i < 200; ++i) {
        const float v = g.next();
        maxStep = std::max(maxStep, std::fabs(v - prev));
        prev = v;
    }
    g.setOpen(false);
    for (int i = 0; i < 3000; ++i) {
        const float v = g.next();
        maxStep = std::max(maxStep, std::fabs(v - prev));
        prev = v;
    }
    CHECK(maxStep < 0.01f);
}
