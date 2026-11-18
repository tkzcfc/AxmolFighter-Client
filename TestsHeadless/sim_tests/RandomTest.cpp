#include "doctest.h"

#include "mugen/core/math/Random.h"

#include <array>
#include <cstdint>
#include <limits>

using namespace mugen;

TEST_CASE("Random seed 12345 has stable golden outputs")
{
    constexpr std::array<uint64_t, 16> kGolden{
        13720838825685603483ULL, 2398916695208396998ULL, 17770384849984869256ULL,
        891717726879801395ULL, 10241316046318454344ULL, 196975429884907396ULL,
        2947371003896198809ULL, 5456629693515947710ULL, 7119811276388220214ULL,
        16797846354857465181ULL, 14422479908006831058ULL, 9909524114151206876ULL,
        15486790527933621595ULL, 17369254722802659825ULL, 2507895171912971169ULL,
        15944840516247072377ULL};
    Random random(12345);
    for (const uint64_t expected : kGolden)
    {
        CHECK(random.next() == expected);
    }
}

TEST_CASE("Random randomN is closed and supports the full int32 interval")
{
    Random random(12345);
    bool sawMinimum = false;
    bool sawMaximum = false;
    for (int index = 0; index < 2000; ++index)
    {
        const int32_t value = random.randomN(-1, 1);
        CHECK(value >= -1);
        CHECK(value <= 1);
        sawMinimum |= value == -1;
        sawMaximum |= value == 1;
        const int32_t wide = random.randomN(std::numeric_limits<int32_t>::min(),
                                            std::numeric_limits<int32_t>::max());
        CHECK(wide >= std::numeric_limits<int32_t>::min());
        CHECK(wide <= std::numeric_limits<int32_t>::max());
    }
    CHECK(sawMinimum);
    CHECK(sawMaximum);
}

TEST_CASE("Random randomF is half open")
{
    Random random(12345);
    const Fixed minimum = "-2.5"_fx;
    const Fixed maximum = "4.0"_fx;
    for (int index = 0; index < 10000; ++index)
    {
        const Fixed value = random.randomF(minimum, maximum);
        CHECK(value >= minimum);
        CHECK(value < maximum);
    }
    CHECK(random.randomF(minimum, minimum) == minimum);
}
