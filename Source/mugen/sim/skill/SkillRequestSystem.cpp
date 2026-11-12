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
            // 死亡、木偶、受击族与待处理受击都不能开新技能（挣脱留到后续切片）。
            if (state.m_state == RoleStateId::Death || state.m_state == RoleStateId::Puppet ||
                state.m_deathRequested || role::isHitState(state.m_state) || hit.m_pendingHitDefId != 0)
                return;
            if (!flags.canControl())
                return;
            for (const auto& button : intent.m_buttons)
                if (button.m_pressType == 1)  // 只处理按下（B2 无抬起释放技能）
                    skill::rules::dealButton(world, entity, button.m_event);
        });
}
}  // namespace mugen::sim
