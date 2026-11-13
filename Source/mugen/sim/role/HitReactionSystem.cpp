// [4.4] HitReactionSystem
// 读：`RoleStateComponent`（受击族、死亡）、`HitStateComponent`、`DisplacementComponent` 事件位
// 写：`HitStateComponent`（硬直/倒地计时）、`DisplacementComponent`（落地反弹策略）、挣脱请求
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/RoleDef.h"

namespace mugen::sim
{
// 系统 HitReactionSystem：4.4 行，见 DESIGN.md §3.2。
// 浮空/下落/倒地中浮空值耗尽时，落地策略改为“停止不反弹”（原版 onDisplacementEvent 返回 isRigidity）。
void runHitReaction(World& world)
{
    world.ecs().each<RoleStateComponent, HitStateComponent, DisplacementComponent, IdentityComponent>(
        [&](Entity entity, const RoleStateComponent& state, const HitStateComponent& hit, DisplacementComponent& disp,
            const IdentityComponent& identity) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            const bool airborneHit = state.m_state == RoleStateId::HitUp || state.m_state == RoleStateId::HitDown ||
                                     state.m_state == RoleStateId::HitFloor;
            const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
            disp.m_noFloorBounce =
                airborneHit && role && hit.m_rigidity <= Fixed{} && role->rigidity > 0;
        });
}
}  // namespace mugen::sim
