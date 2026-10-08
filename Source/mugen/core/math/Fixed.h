#pragma once

#include "../StdC.h"

#include <cstddef>
#include <cstdint>
#include <limits>

NS_MG_BEGIN

namespace fixed_detail
{

struct UInt128
{
    uint64_t high;
    uint64_t low;
};

int64_t multiplyPortable(int64_t lhs, int64_t rhs);
int64_t multiplyIntrinsic(int64_t lhs, int64_t rhs);
int64_t dividePortable(int64_t lhs, int64_t rhs);
int64_t divideIntrinsic(int64_t lhs, int64_t rhs);

}  // namespace fixed_detail

struct Fixed
{
    static constexpr int kFracBits = 20;
    static constexpr int64_t kOne  = int64_t{1} << kFracBits;

    int64_t raw = 0;

    static constexpr Fixed fromRaw(int64_t value) { return Fixed{value}; }

    static constexpr Fixed fromInt(int64_t value)
    {
        MG_ASSERT(value <= (std::numeric_limits<int64_t>::max() / kOne));
        MG_ASSERT(value >= (std::numeric_limits<int64_t>::min() / kOne));
        return fromRaw(value * kOne);
    }

    static Fixed ratio(int64_t numerator, int64_t denominator);

    constexpr Fixed floor() const
    {
        int64_t value = raw / kOne;
        if (raw < 0 && raw % kOne != 0)
        {
            --value;
        }
        return fromRaw(value * kOne);
    }

    constexpr Fixed ceil() const
    {
        int64_t value = raw / kOne;
        if (raw > 0 && raw % kOne != 0)
        {
            ++value;
        }
        return fromRaw(value * kOne);
    }

    constexpr Fixed trunc() const { return fromRaw((raw / kOne) * kOne); }

    constexpr Fixed roundHalfUp() const
    {
        int64_t value = raw / kOne;
        if (raw % kOne >= kOne / 2)
        {
            ++value;
        }
        return fromRaw(value * kOne);
    }

    constexpr int64_t toInt() const { return raw / kOne; }

    // Rendering and UI code only. Simulation code must stay in fixed point.
    float toFloatForRender() const { return static_cast<float>(raw) / static_cast<float>(kOne); }

    friend constexpr Fixed operator+(Fixed lhs, Fixed rhs)
    {
        MG_ASSERT(!((rhs.raw > 0 && lhs.raw > std::numeric_limits<int64_t>::max() - rhs.raw) ||
                    (rhs.raw < 0 && lhs.raw < std::numeric_limits<int64_t>::min() - rhs.raw)));
        return fromRaw(lhs.raw + rhs.raw);
    }

    friend constexpr Fixed operator-(Fixed lhs, Fixed rhs)
    {
        MG_ASSERT(!((rhs.raw < 0 && lhs.raw > std::numeric_limits<int64_t>::max() + rhs.raw) ||
                    (rhs.raw > 0 && lhs.raw < std::numeric_limits<int64_t>::min() + rhs.raw)));
        return fromRaw(lhs.raw - rhs.raw);
    }

    friend Fixed operator*(Fixed lhs, Fixed rhs) { return fromRaw(fixed_detail::multiplyIntrinsic(lhs.raw, rhs.raw)); }

    friend Fixed operator/(Fixed lhs, Fixed rhs) { return fromRaw(fixed_detail::divideIntrinsic(lhs.raw, rhs.raw)); }

    constexpr Fixed operator-() const
    {
        MG_ASSERT(raw != std::numeric_limits<int64_t>::min());
        return fromRaw(-raw);
    }

    constexpr Fixed& operator+=(Fixed rhs) { return *this = *this + rhs; }

    constexpr Fixed& operator-=(Fixed rhs) { return *this = *this - rhs; }

    Fixed& operator*=(Fixed rhs) { return *this = *this * rhs; }

    Fixed& operator/=(Fixed rhs) { return *this = *this / rhs; }

    friend constexpr bool operator==(Fixed, Fixed)  = default;
    friend constexpr auto operator<=>(Fixed, Fixed) = default;
};

namespace fixed_detail
{

consteval Fixed parseLiteral(const char* text, std::size_t length)
{
    bool negative      = false;
    std::size_t cursor = 0;
    if (cursor < length && (text[cursor] == '-' || text[cursor] == '+'))
    {
        negative = text[cursor] == '-';
        ++cursor;
    }

    uint64_t integer = 0;
    bool hasDigit    = false;
    while (cursor < length && text[cursor] >= '0' && text[cursor] <= '9')
    {
        hasDigit             = true;
        const uint64_t digit = static_cast<uint64_t>(text[cursor] - '0');
        if (integer > (static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) / Fixed::kOne) / 10)
        {
            throw "Fixed literal overflow";
        }
        integer = integer * 10 + digit;
        ++cursor;
    }

    uint64_t fraction    = 0;
    uint64_t denominator = 1;
    if (cursor < length && text[cursor] == '.')
    {
        ++cursor;
        while (cursor < length && text[cursor] >= '0' && text[cursor] <= '9')
        {
            hasDigit = true;
            if (denominator > 100000000000000000ULL)
            {
                throw "Fixed literal has too many decimal places";
            }
            fraction = fraction * 10 + static_cast<uint64_t>(text[cursor] - '0');
            denominator *= 10;
            ++cursor;
        }
    }
    if (!hasDigit || cursor != length)
    {
        throw "Invalid Fixed literal";
    }

    uint64_t fractionRaw = 0;
    uint64_t remainder   = fraction;
    for (int bit = 0; bit < Fixed::kFracBits; ++bit)
    {
        remainder *= 2;
        fractionRaw <<= 1;
        if (remainder >= denominator)
        {
            remainder -= denominator;
            fractionRaw |= 1;
        }
    }
    if (remainder * 2 >= denominator)
    {
        ++fractionRaw;
    }

    uint64_t magnitude = integer * static_cast<uint64_t>(Fixed::kOne) + fractionRaw;
    if (negative)
    {
        constexpr uint64_t kMinMagnitude = uint64_t{1} << 63;
        if (magnitude > kMinMagnitude)
        {
            throw "Fixed literal overflow";
        }
        if (magnitude == kMinMagnitude)
        {
            return Fixed::fromRaw(std::numeric_limits<int64_t>::min());
        }
        return Fixed::fromRaw(-static_cast<int64_t>(magnitude));
    }
    if (magnitude > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
    {
        throw "Fixed literal overflow";
    }
    return Fixed::fromRaw(static_cast<int64_t>(magnitude));
}

}  // namespace fixed_detail

consteval Fixed operator""_fx(const char* text, std::size_t length)
{
    return fixed_detail::parseLiteral(text, length);
}

NS_MG_END
