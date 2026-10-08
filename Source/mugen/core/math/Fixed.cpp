#include "Fixed.h"

#include <limits>

#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__)
#    include <intrin.h>
#endif

NS_MG_BEGIN

namespace fixed_detail
{
namespace
{

uint64_t magnitude(int64_t value)
{
    const uint64_t bits = static_cast<uint64_t>(value);
    return value < 0 ? (~bits + 1) : bits;
}

UInt128 multiplyUnsigned(uint64_t lhs, uint64_t rhs)
{
    const uint64_t lhsLow  = static_cast<uint32_t>(lhs);
    const uint64_t lhsHigh = lhs >> 32;
    const uint64_t rhsLow  = static_cast<uint32_t>(rhs);
    const uint64_t rhsHigh = rhs >> 32;

    const uint64_t lowProduct  = lhsLow * rhsLow;
    const uint64_t crossFirst  = lhsHigh * rhsLow;
    const uint64_t crossSecond = lhsLow * rhsHigh;
    const uint64_t highProduct = lhsHigh * rhsHigh;

    uint64_t low  = lowProduct;
    uint64_t high = highProduct;
    uint64_t add  = crossFirst << 32;
    low += add;
    high += (low < add) + (crossFirst >> 32);
    add                   = crossSecond << 32;
    const uint64_t before = low;
    low += add;
    high += (low < before) + (crossSecond >> 32);
    return {high, low};
}

bool discardedFraction(UInt128 value)
{
    return (value.low & ((uint64_t{1} << Fixed::kFracBits) - 1)) != 0;
}

UInt128 shiftRight20(UInt128 value)
{
    return {value.high >> Fixed::kFracBits, (value.low >> Fixed::kFracBits) | (value.high << (64 - Fixed::kFracBits))};
}

int64_t signedResult(UInt128 magnitudeValue, bool negative, bool roundNegativeDown)
{
    MG_ASSERT(magnitudeValue.high == 0 && "Fixed operation overflow");
    uint64_t value = magnitudeValue.low;
    if (negative && roundNegativeDown)
    {
        MG_ASSERT(value != std::numeric_limits<uint64_t>::max() && "Fixed operation overflow");
        ++value;
    }
    const uint64_t limit = negative ? (uint64_t{1} << 63) : static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    MG_ASSERT(value <= limit && "Fixed operation overflow");
    (void)limit;
    if (!negative)
    {
        return static_cast<int64_t>(value);
    }
    if (value == (uint64_t{1} << 63))
    {
        return std::numeric_limits<int64_t>::min();
    }
    return -static_cast<int64_t>(value);
}

UInt128 shiftLeft20(uint64_t value)
{
    return {value >> (64 - Fixed::kFracBits), value << Fixed::kFracBits};
}

UInt128 divideUnsigned(UInt128 numerator, uint64_t denominator)
{
    UInt128 quotient{0, 0};
    uint64_t remainder = 0;
    for (int bit = 127; bit >= 0; --bit)
    {
        const uint64_t inputBit = bit >= 64 ? ((numerator.high >> (bit - 64)) & 1) : ((numerator.low >> bit) & 1);
        const bool overflow     = (remainder >> 63) != 0;
        remainder               = (remainder << 1) | inputBit;
        if (overflow || remainder >= denominator)
        {
            remainder -= denominator;
            if (bit >= 64)
            {
                quotient.high |= uint64_t{1} << (bit - 64);
            }
            else
            {
                quotient.low |= uint64_t{1} << bit;
            }
        }
    }
    return quotient;
}

}  // namespace

int64_t multiplyPortable(int64_t lhs, int64_t rhs)
{
    const bool negative   = (lhs < 0) != (rhs < 0);
    const UInt128 product = multiplyUnsigned(magnitude(lhs), magnitude(rhs));
    const bool roundDown  = negative && discardedFraction(product);
    return signedResult(shiftRight20(product), negative, roundDown);
}

int64_t multiplyIntrinsic(int64_t lhs, int64_t rhs)
{
#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__)
    int64_t high              = 0;
    const uint64_t low        = static_cast<uint64_t>(_mul128(lhs, rhs, &high));
    const uint64_t shiftedLow = __shiftright128(low, static_cast<uint64_t>(high), Fixed::kFracBits);
    const int64_t shiftedHigh = high >> Fixed::kFracBits;
    MG_ASSERT((shiftedHigh == 0 && shiftedLow <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) ||
              (shiftedHigh == -1 && shiftedLow >= (uint64_t{1} << 63)));
    (void)shiftedHigh;
    return static_cast<int64_t>(shiftedLow);
#elif defined(__SIZEOF_INT128__)
    const __int128 product = static_cast<__int128>(lhs) * static_cast<__int128>(rhs);
    const __int128 shifted = product >> Fixed::kFracBits;
    MG_ASSERT(shifted >= std::numeric_limits<int64_t>::min() && shifted <= std::numeric_limits<int64_t>::max() &&
              "Fixed multiplication overflow");
    return static_cast<int64_t>(shifted);
#else
    return multiplyPortable(lhs, rhs);
#endif
}

int64_t dividePortable(int64_t lhs, int64_t rhs)
{
    MG_ASSERT(rhs != 0 && "Fixed division by zero");
    const bool negative    = (lhs < 0) != (rhs < 0);
    const UInt128 quotient = divideUnsigned(shiftLeft20(magnitude(lhs)), magnitude(rhs));
    return signedResult(quotient, negative, false);
}

int64_t divideIntrinsic(int64_t lhs, int64_t rhs)
{
    MG_ASSERT(rhs != 0 && "Fixed division by zero");
#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__)
    if (rhs != std::numeric_limits<int64_t>::min())
    {
        const bool negative     = (lhs < 0) != (rhs < 0);
        const UInt128 numerator = shiftLeft20(magnitude(lhs));
        int64_t remainder       = 0;
        const int64_t quotient  = _div128(static_cast<int64_t>(numerator.high), numerator.low,
                                          static_cast<int64_t>(magnitude(rhs)), &remainder);
        if (!negative)
        {
            MG_ASSERT(quotient >= 0 && "Fixed division overflow");
            return quotient;
        }
        return quotient == std::numeric_limits<int64_t>::min() ? quotient : -quotient;
    }
    return dividePortable(lhs, rhs);
#elif defined(__SIZEOF_INT128__) && !defined(_WIN32)
    const __int128 numerator = static_cast<__int128>(lhs) << Fixed::kFracBits;
    const __int128 quotient  = numerator / static_cast<__int128>(rhs);
    MG_ASSERT(quotient >= std::numeric_limits<int64_t>::min() && quotient <= std::numeric_limits<int64_t>::max() &&
              "Fixed division overflow");
    return static_cast<int64_t>(quotient);
#else
    return dividePortable(lhs, rhs);
#endif
}

}  // namespace fixed_detail

Fixed Fixed::ratio(int64_t numerator, int64_t denominator)
{
    return fromRaw(fixed_detail::divideIntrinsic(numerator, denominator));
}

NS_MG_END
