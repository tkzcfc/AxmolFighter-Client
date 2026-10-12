#include "SkillTriggers.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffEvents.h"
#include "mugen/sim/ability/AbilityPool.h"

// 技能触发接缝的空入口：Buff、能力与效果三类触发器分别由后续待办补齐实现。
namespace mugen::sim
{
// 技能数组下标转换为配置标识，资源和动作信息保留在事件上下文。
void buff::trigger(World& world, Entity entity, BFEvent event, SkillTrigger source)
{
    BuffContext context;
    context.m_needUseTp = source.m_amount;
    if (source.m_actionId != -1) context.m_actionId = source.m_actionId;
    const auto* pool = world.ecs().tryGet<SkillPool>(entity);
    if (pool && source.m_skill >= 0 && static_cast<size_t>(source.m_skill) < pool->m_skills.size())
        context.m_currentSkill = pool->m_skills[source.m_skill].m_configId;
    dispatchEvent(world, entity, event, context, 0);
}
// 向能力系统派发技能生命周期事件。
void ability::trigger(World& world, Entity entity, SkillEvent event, SkillTrigger source)
{
    AbilityContext context;
    context.m_skillId = -1;
    if (const auto* pool = world.ecs().tryGet<SkillPool>(entity))
        if (source.m_skill >= 0 && static_cast<size_t>(source.m_skill) < pool->m_skills.size())
            context.m_skillId = pool->m_skills[source.m_skill].m_configId;
    ability::trigger(world, entity, event == SkillEvent::BeforeCastSkill ? Event::BeforeCastSkill : Event::AfterCastSkill, context);
}
}
