#pragma once

#include "../StdC.h"

#include <cstddef>
#include <cstdint>
#include <limits>

NS_MG_BEGIN

namespace fixed_detail
{

// 用两个无符号半字保存可移植 128 位算术的中间结果。
struct UInt128
{
    // 保存高 64 位。
    uint64_t high;
    // 保存低 64 位。
    uint64_t low;
};

// 使用可移植整数路径计算 Q20 乘法。
int64_t multiplyPortable(int64_t lhs, int64_t rhs);
// 使用编译器内建路径计算 Q20 乘法。
int64_t multiplyIntrinsic(int64_t lhs, int64_t rhs);
// 使用可移植整数路径计算 Q20 除法。
int64_t dividePortable(int64_t lhs, int64_t rhs);
// 使用编译器内建路径计算 Q20 除法。
int64_t divideIntrinsic(int64_t lhs, int64_t rhs);

}  // namespace fixed_detail

// 表示 Q44.20 定点数，战斗逻辑不得转换为浮点数。
struct Fixed
{
    // 指定小数部分使用的二进制位数。
    static constexpr int kFracBits = 20;
    // 表示数值 1 的原始定点值。
    static constexpr int64_t kOne = int64_t{1} << kFracBits;

    // 保存 Q44.20 格式的原始有符号整数。
    int64_t raw = 0;

    // 从已按 Q44.20 缩放的原始整数直接构造。
    static constexpr Fixed fromRaw(int64_t value) { return Fixed{value}; }

    // 从整数构造定点数，并在开发期检查原始值溢出。
    static constexpr Fixed fromInt(int64_t value)
    {
        MG_ASSERT(value <= (std::numeric_limits<int64_t>::max() / kOne));
        MG_ASSERT(value >= (std::numeric_limits<int64_t>::min() / kOne));
        return fromRaw(value * kOne);
    }

    // 使用确定性整数除法构造两个整数的比值。
    static Fixed ratio(int64_t numerator, int64_t denominator);

    // 向负无穷取整到整数倍的定点单位。
    constexpr Fixed floor() const
    {
        int64_t value = raw / kOne;
        if (raw < 0 && raw % kOne != 0)
        {
            --value;
        }
        return fromRaw(value * kOne);
    }

    // 向正无穷取整到整数倍的定点单位。
    constexpr Fixed ceil() const
    {
        int64_t value = raw / kOne;
        if (raw > 0 && raw % kOne != 0)
        {
            ++value;
        }
        return fromRaw(value * kOne);
    }

    // 向零截断到整数倍的定点单位。
    constexpr Fixed trunc() const { return fromRaw((raw / kOne) * kOne); }

    // 按半值向上规则取整到整数倍的定点单位。
    constexpr Fixed roundHalfUp() const
    {
        int64_t value = raw / kOne;
        if (raw % kOne >= kOne / 2)
        {
            ++value;
        }
        return fromRaw(value * kOne);
    }

    // 向零截断并返回对应的整数值。
    constexpr int64_t toInt() const { return raw / kOne; }

    // 仅供渲染和 UI 转换；仿真逻辑必须保持定点数。
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

    // 相乘后右移小数位，并按平台选择确定性整数实现。
    friend Fixed operator*(Fixed lhs, Fixed rhs) { return fromRaw(fixed_detail::multiplyIntrinsic(lhs.raw, rhs.raw)); }

    // 将被除数补足小数位后执行确定性整数除法。
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

// 在编译期解析十进制定点字面量，非法输入以 throw 产生编译期诊断。
consteval Fixed parseLiteral(const char* text, std::size_t length)
{
    // 这里保留 throw，专门让非法字面量在编译期产生诊断。
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

// 将 fx 后缀十进制字面量转换为精确的 Q44.20 值。
consteval Fixed operator""_fx(const char* text, std::size_t length)
{
    return fixed_detail::parseLiteral(text, length);
}

NS_MG_END
