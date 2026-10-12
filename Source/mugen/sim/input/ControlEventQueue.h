#pragma once
#include "PlayerCommand.h"

namespace mugen::sim
{
// 角色等待处理的控制命令队列。
struct ControlEventQueue
{
    // 按输入顺序保存的控制命令。
    std::vector<PlayerCommand> commands;
    // 参与组件快照的命令字段。
    MG_REFLECT(ControlEventQueue, commands)
};
}  // namespace mugen::sim
