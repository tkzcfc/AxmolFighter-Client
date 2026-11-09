// [3.2] PlayerIntentSystem
// 读：`PlayerControl`、`StatusFlags`(canControl)、`Pause`
// 写：`Intent`；未暂停时排空 `PlayerControl.m_commands`
// 消费：`PlayerControl.m_commands`
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
namespace
{
// 控制事件到移动意图模式的映射（CTR_IDLE/CTR_WALK/CTR_RUN）。
MoveMode moveMode(ControlEvent event)
{
    switch (event)
    {
    case ControlEvent::CTR_IDLE:
        return MoveMode::Stop;
    case ControlEvent::CTR_WALK:
        return MoveMode::Walk;
    case ControlEvent::CTR_RUN:
        return MoveMode::Run;
    default:
        return MoveMode::None;
    }
}
}  // namespace

// 系统 PlayerIntentSystem：3.2 行，见 DESIGN.md §3.2。
void runPlayerIntent(World& world)
{
    const bool paused = world.ecs().singleton<RoomState>().m_paused;
    world.ecs().each<PlayerControl, Intent, StatusFlags, Pause>(
        [&](Entity, PlayerControl& control, Intent& intent, const StatusFlags& flags, const Pause& pause) {
            if (!flags.canControl())
                return;
            // 未暂停时排空命令，暂停期间命令保留（R9）。
            if (paused || pause.m_paused)
                return;
            intent.m_buttons.clear();
            for (const auto& command : control.m_commands)
            {
                const MoveMode mode = moveMode(command.controlEvent);
                if (mode != MoveMode::None)
                {
                    intent.m_move.m_mode     = mode;
                    intent.m_move.m_quadrant = command.quadrant;
                    intent.m_move.m_angle    = command.angle;
                }
                else if (static_cast<int32_t>(command.controlEvent) >= static_cast<int32_t>(ControlEvent::CTR_ATKA))
                {
                    intent.m_buttons.push_back({command.controlEvent, command.pressType, command.angle});
                }
            }
            control.m_commands.clear();
        });
}
}  // namespace mugen::sim
