#include "SkillCaster.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/Effect.h"
#include "mugen/sim/buff/BuffHolder.h"
#include "mugen/sim/ability/AbilityHolder.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim::skill
{
// 技能数组下标转换为配置标识，资源和动作信息保留在事件上下文。
void SkillCaster::trigger(BFEvent event, SkillTrigger source)
{
    BuffContext context;
    context.m_needUseTp = source.m_amount;
    if (source.m_actionId != -1)
        context.m_actionId = source.m_actionId;
    const auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity);
    if (pool && source.m_skill >= 0 && static_cast<size_t>(source.m_skill) < pool->m_skills.size())
        context.m_currentSkill = pool->m_skills[source.m_skill].m_configId;
    buff::BuffHolder(m_world, m_entity).dispatchEvent(event, context, 0);
}
// 向能力系统派发技能生命周期事件。
void SkillCaster::trigger(ability::SkillEvent event, SkillTrigger source)
{
    AbilityContext context;
    context.m_skillId = -1;
    if (const auto* pool = m_world.ecs().tryGet<SkillPool>(m_entity))
        if (source.m_skill >= 0 && static_cast<size_t>(source.m_skill) < pool->m_skills.size())
            context.m_skillId = pool->m_skills[source.m_skill].m_configId;
    ability::AbilityHolder(m_world, m_entity)
        .trigger(event == ability::SkillEvent::BeforeCastSkill ? ability::Event::BeforeCastSkill
                                                               : ability::Event::AfterCastSkill,
                 context);
}
// 强制打断按槽首技能和最初施法者筛选模式零特效。
void SkillCaster::clearSkillEffectsByInterrupt(int32_t skillId)
{
    m_world.ecs().each<EffectState>([&](Entity entity, const EffectState& state) {
        if (state.m_owner == m_entity && state.m_baseSkillId == skillId &&
            conf::Config::instance().get<conf::EffectConfig>(state.m_configId).autoRelease == 0)
            m_world.destroy(entity);
    });
}
}  // namespace mugen::sim::skill
