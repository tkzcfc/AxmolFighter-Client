#include "doctest.h"

#include "mugen/core/math/FixedMath.h"

#include <array>

using namespace mugen;

namespace
{

int64_t absolute(int64_t value)
{
    return value < 0 ? -value : value;
}

}

TEST_CASE("FixedMath sqrt is exact for squares and monotonic otherwise")
{
    for (int64_t value = 0; value <= 100; ++value)
    {
        CHECK(FixedMath::sqrt(Fixed::fromInt(value * value)) == Fixed::fromInt(value));
    }
    Fixed previous{};
    for (int64_t raw = 0; raw < Fixed::fromInt(20).raw; raw += 7919)
    {
        const Fixed current = FixedMath::sqrt(Fixed::fromRaw(raw));
        CHECK(current >= previous);
        previous = current;
    }
}

TEST_CASE("FixedMath trigonometry stays within generated reference tolerance")
{
    constexpr int64_t kTolerance = 209;
    struct SinCase
    {
        Fixed input;
        int64_t expected;
    };
    constexpr std::array sinCases{
        SinCase{"12.345"_fx, 224131}, SinCase{"-77.75"_fx, -1024778}, SinCase{"359.9"_fx, -1830}};
    for (const auto& test : sinCases)
    {
        CHECK(absolute((FixedMath::sinDeg(test.input)).raw - test.expected) < kTolerance);
    }
    CHECK(absolute(FixedMath::cosDeg("123.456"_fx).raw - (-578076)) < kTolerance);

    struct AtanCase
    {
        Fixed y;
        Fixed x;
        int64_t expected;
    };
    constexpr std::array atanCases{
        AtanCase{Fixed::fromInt(1), Fixed::fromInt(1), 47185920},
        AtanCase{Fixed::fromInt(1), Fixed::fromInt(-2), 160888205},
        AtanCase{Fixed::fromInt(-3), Fixed::fromInt(-1), -113702285}};
    for (const auto& test : atanCases)
    {
        CHECK(absolute(FixedMath::atan2Deg(test.y, test.x).raw - test.expected) < kTolerance);
    }
}
