// [2.1] CooldownSystem
// 读：`SkillBookComponent`、`WorldClock`
// 写：`SkillBookComponent`（CD、次数）
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
    const Fixed dt = world.ecs().singleton<WorldClock>().dtMs;
    world.ecs().each<SkillBookComponent>([&](Entity entity, SkillBookComponent& book) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        for (auto& skill : book.skills)
        {
            skill.aiLoadCd  = std::max(Fixed{}, skill.aiLoadCd - dt);
            skill.aiCheckCd = std::max(Fixed{}, skill.aiCheckCd - dt);
            if (!skill.timing)
                continue;
            skill.cd = std::max(Fixed{}, skill.cd - dt);
            if (skill.cd == Fixed{})
            {
                skill.timing       = false;
                skill.releaseCount = std::min(skill.releaseCount + 1, skill.releaseMax);
                BuffContext ctx;
                ctx.target  = entity;
                ctx.skillId = skill.configId;
                buff::rules::fire(world, entity, conf::battle::TriggerPoint::SkillColdEnd, ctx);
            }
        }
    });
}
}  // namespace mugen::sim
