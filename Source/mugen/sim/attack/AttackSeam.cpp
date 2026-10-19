#include "AttackSeam.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/Effect.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/buff/BuffHolder.h"

namespace mugen::sim::attack
{
AttackSeam::AttackSeam(World& world, Entity entity) : m_world(world), m_entity(entity) {}
// 从角色动作继承技能来源并立即进入效果动作。
Entity AttackSeam::spawnFromRole(const effect::RoleSpawn& request)
{
    const auto entity = effect::EffectActor::create(m_world, m_entity, request.m_effectId);
    if (entity.value == 0)
        return {};
    auto& state               = m_world.ecs().get<EffectState>(entity);
    state.m_skillId           = request.m_skillId;
    state.m_baseSkillId       = request.m_baseSkillId;
    state.m_slot              = request.m_slot;
    state.m_skillAttributeTag = m_entity;
    effect::EffectActor actor(m_world, entity);
    actor.setCustomPosition(m_entity);
    actor.follow();
    if (request.m_customVector)
    {
        auto& transform   = m_world.ecs().get<Transform>(entity);
        transform.vectorX = request.m_vector.x;
        transform.vectorZ = request.m_vector.z;
    }
    actor.enter();
    return entity;
}
// 动作退出仅销毁模式零，其他效果继续自身生命周期。
void AttackSeam::clearActionEffects(const std::vector<Entity>& effects)
{
    for (const auto entity : effects)
        if (const auto* state = m_world.ecs().tryGet<EffectState>(entity))
            if (conf::Config::instance().get<conf::EffectConfig>(state->m_configId).autoRelease == 0)
                m_world.destroy(entity);
}
// 动作直接向角色池添加效果，保留施放技能来源。
void AttackSeam::addActionBuff(int32_t id, int32_t skillId)
{
    BuffContext context;
    context.m_frontSkill = skillId;
    context.m_applicator = m_entity;
    buff::BuffHolder(m_world, m_entity).addBuff(id, context, m_world.setup().m_fightType);
}
// 动作结束沿用普通移除与消失动画计时。
void AttackSeam::removeActionBuff(int32_t id)
{
    buff::BuffHolder(m_world, m_entity).removeBuff(id);
}
}  // namespace mugen::sim::attack
