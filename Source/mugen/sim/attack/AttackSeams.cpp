#include "AttackSeams.h"
#include "mugen/sim/World.h"
#include "mugen/sim/skill/SkillTriggers.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/buff/BuffPool.h"

// 动作模块与效果、Buff 模块之间的接缝：这里只声明空的接缝入口，
// 具体实现分别由后续的效果（effect）和 Buff 待办补齐。
namespace mugen::sim
{
namespace effect
{
// 从角色动作继承技能来源并立即进入效果动作。
Entity spawnFromRole(World& world, Entity owner, const RoleSpawn& request)
{
    const auto entity = create(world, owner, request.m_effectId);
    if (entity.value == 0) return {};
    auto& state = world.ecs().get<EffectState>(entity);
    state.m_skillId = request.m_skillId;
    state.m_baseSkillId = request.m_baseSkillId;
    state.m_slot = request.m_slot;
    state.m_skillAttributeTag = owner;
    setCustomPosition(world, entity, owner);
    follow(world, entity);
    if (request.m_customVector)
    {
        auto& transform = world.ecs().get<Transform>(entity);
        transform.vectorX = request.m_vector.x;
        transform.vectorZ = request.m_vector.z;
    }
    enter(world, entity);
    return entity;
}
// 动作退出仅销毁模式零，其他效果继续自身生命周期。
void clearActionEffects(World& world, Entity, const std::vector<Entity>& effects)
{
    for (const auto entity : effects)
        if (const auto* state = world.ecs().tryGet<EffectState>(entity))
            if (conf::Config::instance().get<conf::EffectConfig>(state->m_configId).autoRelease == 0)
                world.destroy(entity);
}
// 强制打断按槽首技能和最初施法者筛选模式零特效。
void clearSkillEffectsByInterrupt(World& world, Entity owner, int32_t skillId)
{
    world.ecs().each<EffectState>([&](Entity entity, const EffectState& state) {
        if (state.m_owner == owner && state.m_baseSkillId == skillId &&
            conf::Config::instance().get<conf::EffectConfig>(state.m_configId).autoRelease == 0)
            world.destroy(entity);
    });
}
}
namespace buff
{
// 动作直接向角色池添加效果，保留施放技能来源。
void addActionBuff(World& world, Entity entity, int32_t id, int32_t skillId)
{
    BuffContext context;
    context.m_frontSkill = skillId;
    context.m_applicator = entity;
    addBuff(world, entity, id, context, world.setup().m_fightType);
}
// 动作结束沿用普通移除与消失动画计时。
void removeActionBuff(World& world, Entity entity, int32_t id) { removeBuff(world, entity, id); }
}
}
