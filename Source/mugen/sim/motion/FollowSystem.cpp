// [5.2] FollowSystem
// 读：`FollowComponent`、主人/目标的 `TransformComponent`、`EffectInfoComponent`、`PauseComponent`、`IdentityComponent`
// 写：`TransformComponent`
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/effect/EffectRules.h"

namespace mugen::sim
{
// 系统 FollowSystem：5.2 行，见 DESIGN.md §3.2。位移之后贴合主人/目标最终位置（R12）。
void runFollow(World& world)
{
    const uint64_t frame = world.frame();
    world.ecs().each<FollowComponent, TransformComponent, EffectInfoComponent>(
        [&](Entity entity, const FollowComponent& follow, TransformComponent&, const EffectInfoComponent&) {
            if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
                return;
            if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
                if (pause->m_paused)
                    return;
            const auto& identity = world.ecs().get<IdentityComponent>(entity);
            if (identity.m_born.m_frame == frame && identity.m_born.m_stage > SpawnStage::RoleTimeline)
                return;
            if (follow.m_mode != 1 && follow.m_mode != 2)
                return;
            if (!world.ecs().alive(follow.m_target) || world.ecs().has<DespawningComponent>(follow.m_target) ||
                !world.ecs().has<TransformComponent>(follow.m_target))
                return;
            effect::rules::placeFrom(world, entity, follow.m_target, follow.m_mode == 2);
        });
}
}  // namespace mugen::sim
