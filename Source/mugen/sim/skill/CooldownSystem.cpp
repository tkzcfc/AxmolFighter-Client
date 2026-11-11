// [2.1] CooldownSystem
// 读：`SkillBook`、`WorldClock`
// 写：`SkillBook`（CD、次数）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"

namespace mugen::sim
{
// 系统 CooldownSystem：2.1 行，见 DESIGN.md §3.2。递减技能冷却并恢复次数。
void runCooldown(World& world)
{
    const Fixed dt = world.ecs().singleton<WorldClock>().m_dtMs;
    world.ecs().each<SkillBook>([&](Entity entity, SkillBook& book) {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            return;
        for (auto& skill : book.m_skills)
        {
            if (!skill.m_timing)
                continue;
            skill.m_cd = std::max(Fixed{}, skill.m_cd - dt);
            if (skill.m_cd == Fixed{})
            {
                skill.m_timing       = false;
                skill.m_releaseCount = std::min(skill.m_releaseCount + 1, skill.m_releaseMax);
                BuffContext ctx;
                ctx.m_target  = entity;
                ctx.m_skillId = skill.m_configId;
                buff::rules::fire(world, entity, conf::battle::TriggerPoint::SkillColdEnd, ctx);
            }
        }
    });
}
}  // namespace mugen::sim
