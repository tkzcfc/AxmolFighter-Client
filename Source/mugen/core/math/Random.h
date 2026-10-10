#pragma once

#include "Fixed.h"
#include "mugen/core/reflect/Reflect.h"

NS_MG_BEGIN

// 可快照的确定性伪随机数发生器；每个仿真仅应持有一个实例。
class Random
{
public:
    // 使用固定默认种子初始化。
    Random();
    // 使用调用方提供的种子初始化完整状态。
    explicit Random(uint64_t seed);

    // 用种子重置四字状态。
    void seed(uint64_t s);
    // 生成下一个均匀 64 位伪随机数。
    uint64_t next();
    // 在闭区间端点之间生成定点随机值。
    Fixed randomF(Fixed min, Fixed max);
    // 在闭区间端点之间生成整数随机值。
    int32_t randomN(int32_t min, int32_t max);

private:
    // 使用 SplitMix64 扩展单个种子状态。
    static uint64_t _splitmix64(uint64_t& x);
    // 循环左移内部状态字。
    static uint64_t _rotl(uint64_t v, int k);

    // xoshiro 状态字，必须随快照保存。
    uint64_t m_s0 = 0;
    uint64_t m_s1 = 0;
    uint64_t m_s2 = 0;
    uint64_t m_s3 = 0;

public:
    MG_REFLECT(Random, m_s0, m_s1, m_s2, m_s3)
};

NS_MG_END
