#pragma once
#include "mugen/conf/tables/MapTables.h"
#include "mugen/sim/input/FrameInput.h"

namespace mugen::sim
{
class World;
namespace room
{
// 初始化房间流程；英雄由调用方先创建。
bool enter(World& world, const conf::RoomConfig& config, int32_t stageId, int32_t copyId);
// 将尚未请求过的状态加入顺序队列。
void setExpectRoomState(World& world, mugen::RoomState state);
// 在场景阶段推进房间流程。
void update(World& world);
// 消费帧输入中的复活和返回请求。
void commands(World& world, const FrameInput& input);
// 检查计入清场的敌方角色是否已离场。
bool checkWipeOutEnemys(World& world);
}
}
