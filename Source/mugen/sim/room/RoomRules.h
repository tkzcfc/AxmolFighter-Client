#pragma once
#include "mugen/conf/tables/MapTables.h"

namespace mugen::sim
{
class World;
namespace room::rules
{
// 进入战斗房间：定位玩家、生成怪物并初始化房间状态（移植旧实现 RoomDirector::enter 的定位与怪物出生）。
bool enter(World& world, const conf::RoomConfig& config, int32_t stageId, int32_t copyId);
}  // namespace room::rules
}  // namespace mugen::sim
