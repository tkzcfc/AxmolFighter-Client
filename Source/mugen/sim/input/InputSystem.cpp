// [1.1] InputSystem
// 读：`FrameInput`、`PlayerSlots`
// 写：`PlayerControl.m_commands`；木偶的 `Transform`/`MotionState`/`RoleState`
// 消费：—
// 产生：`RoomRequest`
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 InputSystem：1.1 行，见 DESIGN.md §3.2。
void runInput(World&)
{
}
}  // namespace mugen::sim
