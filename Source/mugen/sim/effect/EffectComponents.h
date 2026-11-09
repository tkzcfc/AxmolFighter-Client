#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/ecs/Entity.h"
#include <vector>

namespace mugen::sim
{
// 特效实体的归属、技能来源与生命周期共享数据。
struct EffectInfo
{
    // 特效配置标识。
    int32_t m_configId = -1;
    // 属性唯一来源（主人）。
    Entity m_owner;
    // 命中特效的目标（受击特效用）。
    Entity m_hitEntity;
    // 当前技能及槽首技能。
    int32_t m_skillId = -1;
    int32_t m_baseSkillId = -1;
    // 技能槽位下标。
    int32_t m_slot = 0;
    // 技能属性来源实体。
    Entity m_skillAttribute;
    // 当前顿帧剩余时间与上一帧冻结状态。
    Fixed m_freezeMs;
    bool m_frozen = false;
    // 是否允许结束后派生后继特效。
    bool m_next = true;
    MG_REFLECT(EffectInfo, m_configId, m_owner, m_hitEntity, m_skillId, m_baseSkillId, m_slot, m_skillAttribute,
               m_freezeMs, m_frozen, m_next)
};
// 特效命中追踪：本帧命中数、命中间隔、已命中目标去重。
struct HitTracker
{
    // 本帧命中次数。
    int32_t m_hitCount = 0;
    // 命中间隔剩余（毫秒）。
    Fixed m_hitInterval;
    // 按目标去重（hitInterval == -1 时）。
    std::vector<Entity> m_hitTargets;
    MG_REFLECT(HitTracker, m_hitCount, m_hitInterval, m_hitTargets)
};
// 特效寿命策略（autoRelease）。
struct Lifetime
{
    // autoRelease：0 片段退出销毁、1 片段放完销毁、2/3 命中目标数达标销毁（3 触发后继）。
    int32_t m_autoRelease = 0;
    MG_REFLECT(Lifetime, m_autoRelease)
};
// 特效跟随：0 不跟随、1 跟随主人、2 跟随目标（贴地）。
struct Follow
{
    // 跟随目标。
    Entity m_target;
    // 跟随模式。
    int32_t m_mode = 0;
    MG_REFLECT(Follow, m_target, m_mode)
};
}  // namespace mugen::sim
