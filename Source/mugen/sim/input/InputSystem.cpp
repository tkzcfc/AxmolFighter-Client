// [1.1] InputSystem
// 读：`FrameInput`、`PlayerSlots`
// 写：`PlayerControl.m_commands`
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 InputSystem：1.1 行，见 DESIGN.md §3.2。把帧输入命令按槽位压入 PlayerControl。
void runInput(World& world)
{
    const auto& input = world.input();
    auto& slots       = world.ecs().singleton<PlayerSlots>().m_entities;
    for (const auto& player : input.players)
    {
        const auto found = slots.find(player.slot);
        if (found == slots.end())
            continue;
        const Entity entity = found->second;
        if (auto* control = world.ecs().tryGet<PlayerControl>(entity))
            for (const auto& command : player.commands)
                if (command.controlEvent != ControlEvent::CTR_NONE)
                    control->m_commands.push_back(command);
    }
}
}  // namespace mugen::sim
