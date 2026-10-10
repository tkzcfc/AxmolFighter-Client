#include "Random.h"

NS_MG_BEGIN

// 使用固定种子构造，避免依赖系统熵或时间。
Random::Random()
{
    seed(0x123456789ABCDEF0ULL);
}

// 用调用方提供的确定性种子构造。
Random::Random(uint64_t s)
{
    seed(s);
}

// 通过 SplitMix64 将单个种子扩展为四个状态字。
void Random::seed(uint64_t s)
{
    m_s0 = _splitmix64(s);
    m_s1 = _splitmix64(s);
    m_s2 = _splitmix64(s);
    m_s3 = _splitmix64(s);
}

// 推进 xoshiro 状态并返回一个伪随机 64 位结果。
uint64_t Random::next()
{
    const uint64_t result = _rotl(m_s1 * 5, 7) * 9;
    const uint64_t t      = m_s1 << 17;

    m_s2 ^= m_s0;
    m_s3 ^= m_s1;
    m_s1 ^= m_s2;
    m_s0 ^= m_s3;

    m_s2 ^= t;
    m_s3 = _rotl(m_s3, 45);

    return result;
}

// 将高位随机比特映射到 [min, max) 的定点区间。
Fixed Random::randomF(Fixed min, Fixed max)
{
    MG_ASSERT(min <= max && "Invalid range for randomF");
    if (min == max)
    {
        return min;
    }
    const Fixed fraction = Fixed::fromRaw(static_cast<int64_t>(next() >> (64 - Fixed::kFracBits)));
    return min + (max - min) * fraction;
}

// 以取模方式映射到两端均包含的整数区间。
int32_t Random::randomN(int32_t min, int32_t max)
{
    MG_ASSERT(min <= max && "Invalid range for randomN");
    if (min == max)
    {
        return min;
    }
    const uint64_t range = static_cast<uint64_t>(static_cast<int64_t>(max) - static_cast<int64_t>(min)) + 1;
    const int64_t result = static_cast<int64_t>(min) + static_cast<int64_t>(next() % range);
    return static_cast<int32_t>(result);
}

// 生成用于初始化 xoshiro 状态的 SplitMix64 输出。
uint64_t Random::_splitmix64(uint64_t& x)
{
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z          = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z          = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// 对状态字执行非零位数的循环左移。
uint64_t Random::_rotl(uint64_t v, int k)
{
    return (v << k) | (v >> (64 - k));
}

NS_MG_END
