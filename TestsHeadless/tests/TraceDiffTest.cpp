#include "../support/TraceDiff.h"
#include <doctest.h>
#include <stdexcept>

TEST_CASE("Effects diff ignores frame state and reports frame offsets")
{
    const std::string left =
        "# header\n"
        "cast role.101.1 10 A 1\n"
        "frame 4\n"
        "ent 4 role 101 1 x 1\n"
        "ev 4 0 damage role.101.1 value 3\n"
        "castend role.101.1 10\n"
        "frame 8\n";
    const std::string right =
        "frame 1\n"
        "cast role.101.1 10 A 1\n"
        "frame 9\n"
        "castend role.101.1 10\n"
        "frame 12\n"
        "ent 12 role 101 1 x 9\n";
    const auto diff = effects_trace::diffEffects(left, right, {});
    CHECK(diff.matched);
    CHECK(diff.report.find("offsets role.101.1 matched 2 nonzero 2") != std::string::npos);
    CHECK(diff.report.find("counts cast a=1 b=1") != std::string::npos);
    CHECK(diff.report.find("mismatch") == std::string::npos);
}

TEST_CASE("Effects diff reports the first mismatch per entity with context")
{
    const std::string left =
        "cast role.101.1 10 A 1\n"
        "frame 2\n"
        "dmg role.101.1 role.61300.2 80 12 0\n"
        "frame 3\n"
        "cast role.200.1 4 A 1\n"
        "frame 4\n";
    const std::string right =
        "cast role.101.1 10 A 1\n"
        "frame 2\n"
        "spawn role.101.1 90\n"
        "frame 3\n"
        "cast role.200.1 4 A 1\n"
        "frame 5\n";
    const auto diff = effects_trace::diffEffects(left, right, {});
    CHECK_FALSE(diff.matched);
    CHECK(diff.report.find("mismatch role.101.1 event 1") != std::string::npos);
    CHECK(diff.report.find("dmg role.101.1 role.61300.2 80 12 0") != std::string::npos);
    CHECK(diff.report.find("spawn role.101.1 90") != std::string::npos);
    CHECK(diff.report.find("mismatch role.200.1") == std::string::npos);
    CHECK(diff.report.find("counts dmg a=1 b=0") != std::string::npos);
    CHECK(diff.report.find("counts spawn a=0 b=1") != std::string::npos);
}

TEST_CASE("Effects diff drops allowlisted lines and keeps buff sign")
{
    const std::string left =
        "buff + role.101.1 12\n"
        "frame 2\n"
        "buff - role.101.1 12\n"
        "frame 3\n"
        "dmg role.101.1 role.2.1 3 9 1\n"
        "frame 4\n"
        "death role.2.1\n"
        "frame 5\n";
    const std::string right =
        "buff + role.101.1 12\n"
        "frame 8\n"
        "buff - role.101.1 12\n"
        "frame 9\n"
        "death role.2.1\n"
        "frame 10\n";
    const auto diff = effects_trace::diffEffects(left, right, {"^dmg "});
    CHECK(diff.matched);
    CHECK(diff.report.find("counts buff+ a=1 b=1") != std::string::npos);
    CHECK(diff.report.find("counts buff- a=1 b=1") != std::string::npos);
    CHECK(diff.report.find("counts dmg a=0 b=0") != std::string::npos);
    CHECK(diff.report.find("counts death a=1 b=1") != std::string::npos);
}

TEST_CASE("Effects diff rejects an invalid allow pattern")
{
    CHECK_THROWS_AS(effects_trace::diffEffects("cast role.1.1 1 A 1\nframe 1\n", "cast role.1.1 1 A 1\nframe 1\n", {"("}),
                    std::invalid_argument);
}
