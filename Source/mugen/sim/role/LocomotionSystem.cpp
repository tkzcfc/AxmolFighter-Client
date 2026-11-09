// [4.5] LocomotionSystem
// 读：`RoleState`（移动/待机/被推/木偶）、`Intent.m_move`、`Stats`(移速)、`StatusFlags`(canMove)
// 写：`Displacement`（控制速度）、`Transform`（朝向）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
// 系统 LocomotionSystem：4.5 行，见 DESIGN.md §3.2。
void runLocomotion(World&)
{
}
}  // namespace mugen::sim
