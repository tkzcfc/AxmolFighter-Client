// [3.2] PlayerIntentSystem
// 读：`PlayerControlComponent`、`StatusFlagsComponent`(canControl)、`PauseComponent`、`RoleStateComponent`
// 写：`IntentComponent`；未暂停时排空 `PlayerControlComponent.commands`
// 消费：`PlayerControlComponent.commands`
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
    const bool paused = world.ecs().singleton<RoomState>().paused;
    world.ecs().each<PlayerControlComponent, IntentComponent, StatusFlagsComponent, PauseComponent, RoleStateComponent>(
        [&](Entity, PlayerControlComponent& control, IntentComponent& intent, const StatusFlagsComponent& flags, const PauseComponent& pause,
            const RoleStateComponent& state) {
            // 暂停期间命令保留（R9）。无法控制时命令直接丢弃，不留到解除后。
            if (paused || pause.paused)
                return;
            if (!flags.canControl())
            {
                control.commands.clear();
                return;
            }
            intent.buttons.clear();
            bool hasMove = false;
            for (const auto& command : control.commands)
            {
                const MoveMode mode = moveMode(command.controlEvent);
                if (mode != MoveMode::None)
                {
                    intent.move.mode     = mode;
                    intent.move.quadrant = command.quadrant;
                    intent.move.angle    = command.angle;
                    hasMove                  = true;
                }
                else if (static_cast<int32_t>(command.controlEvent) >= static_cast<int32_t>(ControlEvent::CTR_ATKA))
                {
                    // PVP 中禁用奥义 I/J/K，避免与普通攻击共用同一条施放路径。
                    if (world.setup().pvp && (command.controlEvent == ControlEvent::CTR_ATKI ||
                                              command.controlEvent == ControlEvent::CTR_ATKJ ||
                                              command.controlEvent == ControlEvent::CTR_ATKK))
                        continue;
                    intent.buttons.push_back({command.controlEvent, command.pressType, command.angle});
                    // 与旧实现 RoleControl.last 一致：施法命令也刷新粘滞摇杆。
                    if (command.quadrant != ControlQuadrant::None)
                    {
                        intent.move.quadrant = command.quadrant;
                        intent.move.angle    = command.angle;
                    }
                }
            }
            // Attack 中只清掉移动模式，保留象限：施法结束不会被拉回 Run，片段 FreeMove 仍能读到粘滞摇杆。
            if (!hasMove && state.state == RoleStateId::Attack)
                intent.move.mode = MoveMode::None;
            control.commands.clear();
        });
}
}  // namespace mugen::sim
