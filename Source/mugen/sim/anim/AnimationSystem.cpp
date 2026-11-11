// [6.1] AnimationSystem
// 读：`MotionState`、`WorldClock`
// 写：`MotionState`（时间、循环、完成位）
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
    world.ecs().each<MotionState>([&](Entity entity, MotionState& motion) {
        if (world.ecs().has<Despawning>(entity) || world.ecs().has<Inactive>(entity))
            return;
        motion.m_completedThisFrame = false;
        if (!motion.m_playing || motion.m_motionName.empty())
            return;
        MotionPlayer player;
        if (!player.bind(motion.m_motionFile) || !player.play(motion.m_motionName, motion.m_entryId, motion.m_loop))
        {
            motion.m_timeMs += dtMs * motion.m_rate;
            return;
        }
        const Fixed duration = Fixed::fromInt(player.getDurationMs());
        motion.m_timeMs += dtMs * motion.m_rate;
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
