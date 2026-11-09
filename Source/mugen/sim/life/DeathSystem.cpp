// [9.1] DeathSystem
// 读：`RoleState`(死亡)、`HitState` 完成位、`Health`、`Identity`
// 写：`Despawning`、`DespawnRequest`
// 消费：`DeathRequest`
// 产生：`DespawnRequest`、`SpawnRequest`(死亡特效)
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
// 系统 DeathSystem：9.1 行，见 DESIGN.md §3.2。死亡动画与死亡位移都结束后生成死亡特效并回收。
void runDeath(World& world)
{
    world.ecs().each<RoleState, HitState, Identity, MotionState>(
        [&](Entity entity, const RoleState& state, const HitState& hit, const Identity& identity,
            const MotionState& motion) {
            if (state.m_state != RoleStateId::Death)
                return;
            if (world.ecs().has<Despawning>(entity))
                return;
            const auto* role = conf::Config::instance().find<conf::battle::RoleDef>(identity.m_configId);
            const int32_t deathDisplacement = role ? role->deathDisplacementId : -1;
            const bool noDisplacement       = deathDisplacement == -1 || deathDisplacement == 0;
            // 死亡体未进入（位移未设置）时等待；进入后动画与位移都完成再回收。
            const bool deathBody = motion.m_motionName == role::motionName(RoleStateId::Death);
            if (noDisplacement || (deathBody && hit.m_animationEnd && hit.m_displacementEnd))
            {
                if (role && role->deathEffectId > 0)
                    effect::rules::spawnFromRole(world, entity, role->deathEffectId, {});
                world.ecs().add(entity, Despawning{});
                world.ecs().singleton<EventQueue<DespawnRequest>>().push({entity, DespawnReason::DeathEnd});
            }
        });
}
}  // namespace mugen::sim
