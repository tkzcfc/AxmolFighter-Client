// [4.1] SkillRequestSystem
// 读：`Intent`、`SkillBook`、`SkillCast`、`RoleState`、`Health`、`StatusFlags`
// 写：`SkillCast`（当前施法/缓存下一招）
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
    world.ecs().each<Intent, SkillBook, SkillCast, RoleState, StatusFlags>(
        [&](Entity entity, const Intent& intent, const SkillBook&, const SkillCast&, const RoleState& state,
            const StatusFlags& flags) {
            if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
                return;
            if (state.m_state == RoleStateId::Death || state.m_state == RoleStateId::Puppet)
                return;
            if (!flags.canControl())
                return;
            for (const auto& button : intent.m_buttons)
                if (button.m_pressType == 1)  // 只处理按下（B2 无抬起释放技能）
                    skill::rules::dealButton(world, entity, button.m_event);
        });
}
}  // namespace mugen::sim
