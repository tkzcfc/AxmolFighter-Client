#pragma once
#include "mugen/sim/WorldSetup.h"
#include "mugen/core/ecs/Entity.h"
#include <vector>

namespace mugen::sim
{
class World;
// 按世界配置执行统一战斗引导：初始化世界、出生全部玩家并进入房间。
// 返回按配置顺序创建的玩家实体；失败时返回空向量（移植旧实现 bootstrapBattle）。
std::vector<Entity> bootstrapBattle(World& world, const WorldSetup& setup);
}  // namespace mugen::sim
