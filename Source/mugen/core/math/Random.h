#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

/**
 * 可序列化的伪随机数生成器，基于 xoshiro256** 算法。
 *
 * 状态完全由 4 个 uint64_t 组成，序列化/反序列化后可精确还原随机序列，
 *
 * 用法：
 *   mugen::Random rng;
 *   rng.seed(12345);
 *   int32_t v = rng.nextInt(0, 100);   // [0, 100]
 *   float   f = rng.nextFloat();        // [0.0f, 1.0f)
 */
class Random : public Object
{
public:
    typedef Object Super;

    Random();
    explicit Random(uint64_t seed);

    // 用给定种子重置状态
    void seed(uint64_t s);

    // 返回 [0, UINT64_MAX] 的原始 64 位随机数
    uint64_t next();

    // 返回 [0, 1) 的 float
    float nextFloat();

    // 返回 [min, max] 的 int32_t（含两端）
    int32_t nextInt(int32_t min, int32_t max);

    // 返回 [min, max) 的 float
    float nextFloat(float min, float max);

    MG_DEFINE_SERIALIZABLE(m_s0, m_s1, m_s2, m_s3);

private:
    static uint64_t _splitmix64(uint64_t& x);
    static uint64_t _rotl(uint64_t v, int k);

    uint64_t m_s0 = 0;
    uint64_t m_s1 = 0;
    uint64_t m_s2 = 0;
    uint64_t m_s3 = 0;
};

NS_MG_END
