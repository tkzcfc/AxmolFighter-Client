// [4.1] SkillRequestSystem
// 读：`IntentComponent`、`SkillBookComponent`、`SkillCastComponent`、`RoleStateComponent`、`HitStateComponent`、`HealthComponent`、`StatusFlagsComponent`
// 写：`SkillCastComponent`（当前施法/缓存下一招）、`ActionTimelineComponent`
// 消费：—
// 产生：`CastEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/skill/SkillRules.h"

namespace mugen::sim
{
// 系统 SkillRequestSystem：4.1 行，见 DESIGN.md §3.2。把按键意图转换为施法请求。
void runSkillRequest(World& world)
{
    world.ecs().each<IntentComponent, SkillBookComponent, SkillCastComponent, RoleStateComponent, HitStateComponent, StatusFlagsComponent>(
        [&](Entity entity, const IntentComponent& intent, const SkillBookComponent&, const SkillCastComponent&, const RoleStateComponent& state,
            const HitStateComponent& hit, const StatusFlagsComponent& flags) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            // 死亡、木偶、挤开、待处理受击不能开新技能；受击中的挣脱由 isAllowCast 只放行 L。
            if (state.state == RoleStateId::Death || state.state == RoleStateId::Puppet ||
                state.state == RoleStateId::Jostled || state.deathRequested || hit.pendingHitDefId != 0)
                return;
            if (world.ecs().has<HealthComponent>(entity))
            {
                const auto& health = world.ecs().get<HealthComponent>(entity);
                if (health.dead || health.hp <= Fixed{})
                    return;
            }
            if (!flags.canControl())
                return;
            for (const auto& button : intent.buttons)
                if (button.pressType == 1)  // 只处理按下（B2 无抬起释放技能）
                    skill::rules::dealButton(world, entity, button.event);
        });
}
}  // namespace mugen::sim
