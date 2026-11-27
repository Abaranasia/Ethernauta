#include <ethernauta/ParamSmoother.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

using ethernauta::ParamSmoother;

TEST_CASE("ParamSmoother converges monotonically to its target", "[smoother]")
{
    ParamSmoother s;
    s.prepare(48000.0, 0.01);
    s.snapTo(0.0f);
    s.setTarget(1.0f);
    float prev = s.next();
    for (int i = 0; i < 48000; ++i) {
        const float v = s.next();
        REQUIRE(v >= prev);
        REQUIRE(v <= 1.0f);
        prev = v;
    }
    CHECK(prev == 1.0f); // snaps exactly once close enough
}

TEST_CASE("ParamSmoother reaches ~63% after one time constant", "[smoother]")
{
    ParamSmoother s;
    s.prepare(48000.0, 0.01);
    s.snapTo(0.0f);
    s.setTarget(1.0f);
    float v = 0.0f;
    for (int i = 0; i < 480; ++i) v = s.next();
    CHECK(v == Catch::Approx(1.0 - std::exp(-1.0)).margin(0.01));
}

TEST_CASE("ParamSmoother snapTo jumps immediately and holds", "[smoother]")
{
    ParamSmoother s;
    s.prepare(44100.0, 0.05);
    s.snapTo(0.5f);
    CHECK(s.next() == 0.5f);
    CHECK(s.next() == 0.5f);
    CHECK(s.isSettled());
    s.setTarget(0.25f);
    CHECK_FALSE(s.isSettled());
}
