#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"
#include <vector>

namespace mugen::sim
{
// 召唤物归属与寿命。标准伤害递归取 m_root。
struct SummonComponent
{
    // 直接主人。
    Entity m_owner;
    // 最终主人（主人也是召唤时取其根）。
    Entity m_root;
    // 剩余寿命（毫秒）。
    Fixed m_remainingMs;
    MG_REFLECT(SummonComponent, m_owner, m_root, m_remainingMs)
};
// 攻击变身各形态实体的链接。每个成员持有一份成员表与自身下标。
struct TransformLinkComponent
{
    // 全部形态实体（创建顺序）。
    std::vector<Entity> m_members;
    // 本实体在成员表中的下标。
    int32_t m_index = 0;
    MG_REFLECT(TransformLinkComponent, m_members, m_index)
};
}  // namespace mugen::sim
