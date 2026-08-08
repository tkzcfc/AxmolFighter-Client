#include "Random.h"

NS_MG_BEGIN

// ─── 初始化 ─────────────────────────────────────────────────

Random::Random()
{
    seed(0x123456789ABCDEF0ULL);
}

Random::Random(uint64_t s)
{
    seed(s);
}

void Random::seed(uint64_t s)
{
    // splitmix64 展开 4 个状态，避免全零初始状态
    m_s0 = _splitmix64(s);
    m_s1 = _splitmix64(s);
    m_s2 = _splitmix64(s);
    m_s3 = _splitmix64(s);
}

// ─── 核心算法：xoshiro256** ──────────────────────────────────

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

// ─── 公共接口 ────────────────────────────────────────────────

float Random::nextFloat()
{
    // 取高 23 位映射到 [1.0, 2.0)，再减 1 得到 [0.0, 1.0)
    const uint32_t bits = static_cast<uint32_t>(next() >> 41);
    const uint32_t ieee = (127u << 23) | bits;
    float result;
    static_assert(sizeof(float) == sizeof(uint32_t), "float must be 32-bit");
    memcpy(&result, &ieee, sizeof(float));
    return result - 1.0f;
}

int32_t Random::nextInt(int32_t min, int32_t max)
{
    MG_ASSERT(min <= max && "Invalid range for nextInt");
    if (min >= max)
    {
        return min;
    }
    const uint64_t range = static_cast<uint64_t>(max - min) + 1ULL;
    return min + static_cast<int32_t>(next() % range);
}

float Random::nextFloat(float min, float max)
{
    MG_ASSERT(min <= max && "Invalid range for nextFloat");
    if (min >= max)
    {
        return min;
    }
    return min + nextFloat() * (max - min);
}

// ─── 私有工具 ────────────────────────────────────────────────

uint64_t Random::_splitmix64(uint64_t& x)
{
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z          = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z          = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

uint64_t Random::_rotl(uint64_t v, int k)
{
    return (v << k) | (v >> (64 - k));
}

NS_MG_END
