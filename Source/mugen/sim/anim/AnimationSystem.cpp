// [6.1] AnimationSystem
// 读：`MotionStateComponent`、`WorldClock`
// 写：`MotionStateComponent`（时间、循环、完成位）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/avatar/MotionPlayer.h"

namespace mugen::sim
{
// 系统 AnimationSystem：6.1 行，见 DESIGN.md §3.2。绑定 MotionPlayer 推进时间并写完成位（R10）。
void runAnimation(World& world)
{
    const Fixed dtMs = world.ecs().singleton<WorldClock>().m_dtMs;
    world.ecs().each<MotionStateComponent>([&](Entity entity, MotionStateComponent& motion) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        const auto& born = world.ecs().get<IdentityComponent>(entity).m_born;
        if (born.m_frame == world.frame() && born.m_stage > SpawnStage::RoleTimeline)
            return;
        if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
            if (pause->m_paused)
                return;
        motion.m_completedThisFrame = false;
        if (!motion.m_playing || motion.m_motionName.empty())
            return;
        const Fixed actionScale = world.ecs().has<StatsComponent>(entity)
                                      ? world.ecs().get<StatsComponent>(entity).finalStat(202)
                                      : Fixed::fromInt(1);
        MotionPlayer player;
        if (!player.bind(motion.m_motionFile) || !player.play(motion.m_motionName, motion.m_entryId, motion.m_loop))
        {
            motion.m_timeMs += dtMs * motion.m_rate * actionScale;
            return;
        }
        const Fixed duration = Fixed::fromInt(player.getDurationMs());
        motion.m_timeMs += dtMs * motion.m_rate * actionScale;
        if (duration > Fixed{})
        {
            if (motion.m_timeMs >= duration && (motion.m_loop || motion.m_completedLoops == 0))
            {
                motion.m_completedLoops += motion.m_loop ? motion.m_timeMs.raw / duration.raw : 1;
                motion.m_completedThisFrame = true;
            }
            motion.m_timeMs = motion.m_loop ? Fixed::fromRaw(motion.m_timeMs.raw % duration.raw)
                                            : std::min(motion.m_timeMs, duration);
        }
    });
}
}  // namespace mugen::sim
