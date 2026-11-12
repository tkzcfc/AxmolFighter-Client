#pragma once
#include "mugen/sim/base/SpawnStage.h"
#include "mugen/sim/Events.h"

namespace mugen::sim
{
class World;
namespace life::rules
{
// 按请求装配一个实体（R2/R3）；TransformSystem 直接实体化新形态时也走这里。
Entity materialize(World& world, const SpawnRequest& request, SpawnStage stage);
// 排空 SpawnRequest 队列并按出生阶段实体化；实体创建顺序 = 入队顺序。
void spawnFlush(World& world, SpawnStage stage);
// 打上 DespawningComponent 并追加 DespawnRequest（R4）。已标记的实体不重复入队。
void despawn(World& world, Entity entity, DespawnReason reason);
// 把来源身上 inherit 含 flag 的 Buff 同步准入到目标（1 召唤 / 2 变身）。
void inheritBuffs(World& world, Entity from, Entity to, int32_t flag);
}  // namespace life::rules
}  // namespace mugen::sim
