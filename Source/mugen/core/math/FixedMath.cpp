#include "FixedMath.h"

#include <cstdint>

NS_MG_BEGIN

namespace FixedMath
{
namespace
{

#include "FixedTables.inc"

fixed_detail::UInt128 multiplyUnsigned(uint64_t lhs, uint64_t rhs)
{
    const uint64_t lhsLow       = static_cast<uint32_t>(lhs);
    const uint64_t lhsHigh      = lhs >> 32;
    const uint64_t rhsLow       = static_cast<uint32_t>(rhs);
    const uint64_t rhsHigh      = rhs >> 32;
    const uint64_t lowProduct   = lhsLow * rhsLow;
    const uint64_t first        = lhsHigh * rhsLow;
    const uint64_t second       = lhsLow * rhsHigh;
    uint64_t low                = lowProduct;
    uint64_t high               = lhsHigh * rhsHigh;
    const uint64_t firstShifted = first << 32;
    low += firstShifted;
    high += (low < firstShifted) + (first >> 32);
    const uint64_t before = low;
    low += second << 32;
    high += (low < before) + (second >> 32);
    return {high, low};
}

bool lessOrEqual(fixed_detail::UInt128 lhs, fixed_detail::UInt128 rhs)
{
    return lhs.high < rhs.high || (lhs.high == rhs.high && lhs.low <= rhs.low);
}

Fixed interpolate(const int32_t* table, int index, int64_t fractionRaw)
{
    const int64_t first = table[index];
    const int64_t delta = static_cast<int64_t>(table[index + 1]) - first;
    return Fixed::fromRaw(first + fixed_detail::multiplyIntrinsic(delta, fractionRaw));
}

uint64_t absRaw(Fixed value)
{
    const uint64_t bits = static_cast<uint64_t>(value.raw);
    return value.raw < 0 ? (~bits + 1) : bits;
}

uint64_t fractionalBits(uint64_t numerator, uint64_t denominator, int bitCount)
{
    uint64_t result    = 0;
    uint64_t remainder = numerator;
    for (int bit = 0; bit < bitCount; ++bit)
    {
        const bool overflow = (remainder >> 63) != 0;
        remainder <<= 1;
        result <<= 1;
        if (overflow || remainder >= denominator)
        {
            remainder -= denominator;
            result |= 1;
        }
    }
    return result;
}

}  // namespace

Fixed sqrt(Fixed value)
{
    MG_ASSERT(value.raw >= 0 && "Fixed sqrt requires a non-negative value");
    if (value.raw == 0)
    {
        return Fixed{};
    }
    const uint64_t raw = static_cast<uint64_t>(value.raw);
    const fixed_detail::UInt128 target{raw >> (64 - Fixed::kFracBits), raw << Fixed::kFracBits};
    uint64_t low  = 0;
    uint64_t high = uint64_t{1} << 42;
    while (low + 1 < high)
    {
        const uint64_t middle = low + (high - low) / 2;
        if (lessOrEqual(multiplyUnsigned(middle, middle), target))
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return Fixed::fromRaw(static_cast<int64_t>(low));
}

Fixed sinDeg(Fixed degrees)
{
    constexpr int64_t kPeriod = 360 * Fixed::kOne;
    int64_t normalized        = degrees.raw % kPeriod;
    if (normalized < 0)
    {
        normalized += kPeriod;
    }
    const int index = static_cast<int>(normalized / Fixed::kOne);
    return interpolate(kSinDegTable, index, normalized % Fixed::kOne);
}

Fixed cosDeg(Fixed degrees)
{
    return sinDeg(degrees + Fixed::fromInt(90));
}

Fixed atan2Deg(Fixed y, Fixed x)
{
    if (x.raw == 0)
    {
        if (y.raw > 0)
        {
            return Fixed::fromInt(90);
        }
        if (y.raw < 0)
        {
            return Fixed::fromInt(-90);
        }
        return Fixed{};
    }

    const uint64_t ax          = absRaw(x);
    const uint64_t ay          = absRaw(y);
    const bool steep           = ay > ax;
    const uint64_t minor       = steep ? ax : ay;
    const uint64_t major       = steep ? ay : ax;
    const uint64_t scaledRatio = fractionalBits(minor, major, Fixed::kFracBits + 10);
    const uint64_t index       = scaledRatio >> Fixed::kFracBits;
    Fixed angle                = Fixed::fromRaw(kAtanDegTable[index]);
    if (index < 1024)
    {
        const int64_t fraction = static_cast<int64_t>(scaledRatio & (Fixed::kOne - 1));
        angle                  = interpolate(kAtanDegTable, static_cast<int>(index), fraction);
    }
    if (steep)
    {
        angle = Fixed::fromInt(90) - angle;
    }
    if (x.raw < 0)
    {
        angle = Fixed::fromInt(180) - angle;
    }
    if (y.raw < 0)
    {
        angle = -angle;
    }
    return angle;
}

}  // namespace FixedMath

NS_MG_END
