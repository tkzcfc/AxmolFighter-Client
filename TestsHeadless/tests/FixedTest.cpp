#include "doctest.h"

#include "mugen/core/math/Fixed.h"
#include "mugen/core/math/FixedVec2.h"
#include "mugen/core/math/FixedVec3.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>

using namespace mugen;

TEST_CASE("Fixed literal and rounding semantics")
{
    constexpr Fixed parsed = "0.0022"_fx;
    static_assert(parsed.raw == 2307);
    CHECK(parsed.raw == 2307);
    CHECK("1.5"_fx.roundHalfUp().toInt() == 2);
    CHECK("-1.5"_fx.roundHalfUp().toInt() == -1);
    CHECK("-1.25"_fx.floor().toInt() == -2);
    CHECK("-1.25"_fx.ceil().toInt() == -1);
    CHECK("-1.25"_fx.trunc().toInt() == -1);
    CHECK("-1.25"_fx.toInt() == -1);
}

TEST_CASE("Fixed multiply and divide truncate in the required direction")
{
    CHECK((Fixed::fromRaw(-3) * Fixed::fromRaw(1)).raw == -1);
    CHECK((Fixed::fromRaw(3) * Fixed::fromRaw(-1)).raw == -1);
    CHECK((Fixed::fromInt(-1) / Fixed::fromInt(3)).raw == -349525);
    CHECK((Fixed::fromInt(1) / Fixed::fromInt(-3)).raw == -349525);
    CHECK((Fixed::fromInt(-1) / Fixed::fromInt(-3)).raw == 349525);
    CHECK(Fixed::ratio(1, 2).raw == Fixed::kOne / 2);
    CHECK((Fixed::fromRaw(std::numeric_limits<int64_t>::max()) * Fixed::fromRaw(1)).raw
          == std::numeric_limits<int64_t>::max() >> Fixed::kFracBits);
}

TEST_CASE("Fixed intrinsic and portable 128 bit paths are bit identical")
{
    uint64_t state = 0x8f3f73b5cf1c9adeULL;
    auto next = [&state]() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    };
    for (int index = 0; index < 10000; ++index)
    {
        const int64_t lhs = static_cast<int64_t>(next() % (uint64_t{1} << 42)) - (int64_t{1} << 41);
        int64_t rhs = static_cast<int64_t>(next() % (uint64_t{1} << 22)) - (int64_t{1} << 21);
        if (rhs == 0)
        {
            rhs = 1;
        }
        CHECK(fixed_detail::multiplyIntrinsic(lhs, rhs) == fixed_detail::multiplyPortable(lhs, rhs));
        CHECK(fixed_detail::divideIntrinsic(lhs, rhs) == fixed_detail::dividePortable(lhs, rhs));
    }
}

TEST_CASE("Fixed vectors use deterministic component math")
{
    const FixedVec2 vec2{Fixed::fromInt(3), Fixed::fromInt(4)};
    CHECK(vec2.length() == Fixed::fromInt(5));
    const FixedVec3 vec3{Fixed::fromInt(2), Fixed::fromInt(3), Fixed::fromInt(6)};
    CHECK(vec3.length() == Fixed::fromInt(7));
}

#if !defined(NDEBUG)
TEST_CASE("Fixed multiplication overflow fires MG_ASSERT in debug")
{
    const std::filesystem::path executable = std::filesystem::absolute("mugen_tests.exe");
    const std::string command = "\"" + executable.string() + "\" --fixed-overflow-probe > NUL 2>&1";
    CHECK(std::system(command.c_str()) != 0);
}
#endif
