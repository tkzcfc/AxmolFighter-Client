#pragma once

#include "World.h"
#include <vector>

namespace mugen::sim_legacy
{
// 按世界配置执行统一战斗引导：初始化世界、出生全部玩家并进入房间。
// 返回按配置顺序创建的玩家实体；失败时返回空向量。
std::vector<Entity> bootstrapBattle(World& world, const WorldSetup& setup);
}  // namespace mugen::sim_legacy
