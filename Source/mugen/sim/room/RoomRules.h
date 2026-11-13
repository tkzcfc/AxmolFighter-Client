#pragma once
#include "mugen/sim/room/RoomComponents.h"
#include "mugen/sim/motion/MotionComponents.h"
#include "mugen/conf/tables/MapTables.h"
#include "mugen/sim/base/SpawnStage.h"

namespace mugen::sim
{
class World;
struct SpawnRequest;
namespace room::rules
{
// 进入战斗房间：定位玩家、生成怪物与房间实体并初始化房间状态机（移植旧实现 RoomDirector::enter）。
bool enter(World& world, const conf::RoomConfig& config, int32_t stageId, int32_t copyId);
// 按房间配置生成障碍、物品与传送门（移植旧实现 RoomDirector::load）。
void load(World& world, const conf::RoomConfig& config);
// 生成单个障碍物并返回实体。
Entity spawnObstacle(World& world, int32_t id, const TransformComponent& transform, int32_t level);
// 生成单个物品并返回实体；spurt 为 true 时按抛射阶段出生。
Entity spawnGoods(World& world, int32_t id, const TransformComponent& transform, bool spurt);
// 生成单个传送门并返回实体。
Entity spawnPortal(World& world, int32_t id, const TransformComponent& transform, int32_t destRoom, int32_t toward,
                   int32_t limit, const FixedVec3& destination);
// 结算实体的死亡掉落（怪物药品与指定物品，移植旧实现 RoomDirector::drop）。
void drop(World& world, Entity entity);
// 胜利后开放全部关闭状态的传送门。
void openPortals(World& world);
// 按生成请求装配房间实体（物品/障碍/传送门），由 SpawnFlushSystem 在同步点调用。
Entity materialize(World& world, const SpawnRequest& request, SpawnStage stage);
}  // namespace room::rules
}  // namespace mugen::sim
