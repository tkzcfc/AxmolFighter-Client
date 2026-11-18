#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/ecs/Entity.h"
#include <vector>

namespace mugen::sim
{
// 特效实体的归属、技能来源与生命周期共享数据。
struct EffectInfoComponent
{
    // 特效配置标识。
    int32_t configId = -1;
    // 属性唯一来源（主人）。
    Entity owner;
    // 命中特效的目标（受击特效用）。
    Entity hitEntity;
    // 当前技能及槽首技能。
    int32_t skillId = -1;
    int32_t baseSkillId = -1;
    // 技能槽位下标。
    int32_t slot = 0;
    // 技能属性来源实体（伤害公式读它的属性；出生时等于主人）。
    Entity attributeSource;
    // 是否允许结束后派生后继特效。
    bool next = true;
    MG_REFLECT(EffectInfoComponent, configId, owner, hitEntity, skillId, baseSkillId, slot, attributeSource,
               next)
};
// 特效命中追踪：本帧命中数、命中间隔、已命中目标去重。
struct HitTrackerComponent
{
    // 本帧命中次数。
    int32_t hitCount = 0;
    // 命中间隔剩余（毫秒）。
    Fixed hitInterval;
    // 按目标去重（hitInterval == -1 时）。
    std::vector<Entity> hitTargets;
    MG_REFLECT(HitTrackerComponent, hitCount, hitInterval, hitTargets)
};
// 特效寿命策略（autoRelease）。
struct LifetimeComponent
{
    // autoRelease：0 片段退出销毁、1 片段放完销毁、2/3 命中目标数达标销毁（3 触发后继）。
    int32_t autoRelease = 0;
    MG_REFLECT(LifetimeComponent, autoRelease)
};
// 特效跟随：0 不跟随、1 跟随主人、2 跟随目标（贴地）。
struct FollowComponent
{
    // 跟随目标。
    Entity target;
    // 跟随模式。
    int32_t mode = 0;
    MG_REFLECT(FollowComponent, target, mode)
};
}  // namespace mugen::sim
