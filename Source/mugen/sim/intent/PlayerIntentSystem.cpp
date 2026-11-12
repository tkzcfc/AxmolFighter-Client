// [3.2] PlayerIntentSystem
// 读：`PlayerControlComponent`、`StatusFlagsComponent`(canControl)、`PauseComponent`
// 写：`IntentComponent`；未暂停时排空 `PlayerControlComponent.m_commands`
// 消费：`PlayerControlComponent.m_commands`
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
    world.ecs().each<PlayerControlComponent, IntentComponent, StatusFlagsComponent, PauseComponent>(
        [&](Entity, PlayerControlComponent& control, IntentComponent& intent, const StatusFlagsComponent& flags, const PauseComponent& pause) {
            // 暂停期间命令保留（R9）。无法控制时命令直接丢弃，不留到解除后。
            if (paused || pause.m_paused)
                return;
            if (!flags.canControl())
            {
                control.m_commands.clear();
                return;
            }
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
