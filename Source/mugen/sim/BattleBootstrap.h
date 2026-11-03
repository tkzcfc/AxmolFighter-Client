#pragma once

#include "World.h"

namespace mugen::sim
{
// 按世界配置执行统一战斗引导：初始化世界、出生玩家并进入房间。
Entity bootstrapBattle(World& world, const WorldSetup& setup);
}  // namespace mugen::sim
