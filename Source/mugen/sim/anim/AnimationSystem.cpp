// [6.1] AnimationSystem
// 读：`MotionState`、`WorldClock`、`RoleState`(HitSwitch 打断)
// 写：`MotionState`（时间、循环、完成位）、`RoleState`(HitSwitch 打断)
// 消费：—
// 产生：`StateEvent`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/avatar/MotionPlayer.h"

namespace mugen::sim
{
// 系统 AnimationSystem：6.1 行，见 DESIGN.md §3.2。绑定 MotionPlayer 推进时间并写完成位（R10）。
// HitSwitch 打断按旧实现 motionUpdate 内 animationComplete 同一帧 abort（不进 RoleFsm 的下一帧读）。
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
                // HitSwitch 打断：受击恢复动作放完时立刻回到待机状态。
                if (auto* state = world.ecs().tryGet<RoleState>(entity))
                {
                    if (state->m_state == RoleStateId::HitSwitch)
                    {
                        state->m_state = RoleStateId::Idle;
                        StateEvent event;
                        event.m_entity = entity;
                        event.m_from   = static_cast<int32_t>(RoleStateId::HitSwitch);
                        event.m_to     = static_cast<int32_t>(RoleStateId::Idle);
                        world.ecs().singleton<EventQueue<StateEvent>>().push(event);
                    }
                }
            }
            motion.m_timeMs = motion.m_loop ? Fixed::fromRaw(motion.m_timeMs.raw % duration.raw)
                                            : std::min(motion.m_timeMs, duration);
        }
    });
}
}  // namespace mugen::sim
