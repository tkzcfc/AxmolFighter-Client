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
    const Fixed dtMs = world.ecs().singleton<WorldClock>().dtMs;
    world.ecs().each<MotionStateComponent>([&](Entity entity, MotionStateComponent& motion) {
        if (world.ecs().has<DespawningComponent>(entity) || world.ecs().has<InactiveComponent>(entity))
            return;
        const auto& born = world.ecs().get<IdentityComponent>(entity).born;
        if (born.frame == world.frame() && born.stage > SpawnStage::RoleTimeline)
            return;
        if (const PauseComponent* pause = world.ecs().tryGet<PauseComponent>(entity))
            if (pause->paused)
                return;
        motion.completedThisFrame = false;
        if (!motion.playing || motion.motionName.empty())
            return;
        const Fixed actionScale = world.ecs().has<StatsComponent>(entity)
                                      ? world.ecs().get<StatsComponent>(entity).finalStat(202)
                                      : Fixed::fromInt(1);
        MotionPlayer player;
        if (!motion.motionFile.empty() &&
            (!player.bind(motion.motionFile) || !player.play(motion.motionName, motion.entryId, motion.loop)))
        {
            motion.timeMs += dtMs * motion.rate * actionScale;
            if (!motion.loop && motion.completedLoops == 0)
            {
                motion.completedLoops     = 1;
                motion.completedThisFrame = true;
            }
            return;
        }
        if (motion.motionFile.empty())
        {
            motion.timeMs += dtMs * motion.rate * actionScale;
            return;
        }
        const Fixed duration = Fixed::fromInt(player.getDurationMs());
        motion.timeMs += dtMs * motion.rate * actionScale;
        if (duration > Fixed{})
        {
            if (motion.timeMs >= duration && (motion.loop || motion.completedLoops == 0))
            {
                motion.completedLoops += motion.loop ? motion.timeMs.raw / duration.raw : 1;
                motion.completedThisFrame = true;
            }
            motion.timeMs = motion.loop ? Fixed::fromRaw(motion.timeMs.raw % duration.raw)
                                            : std::min(motion.timeMs, duration);
        }
        else if (!motion.loop && motion.completedLoops == 0)
        {
            motion.completedLoops     = 1;
            motion.completedThisFrame = true;
        }
    });
}
}  // namespace mugen::sim
