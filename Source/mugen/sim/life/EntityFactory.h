#pragma once
#include "mugen/sim/base/SpawnStage.h"

namespace mugen::sim
{
class World;
namespace life::rules
{
// 排空 SpawnRequest 队列并按出生阶段实体化（R2/R3）；实体创建顺序 = 入队顺序。
void spawnFlush(World& world, SpawnStage stage);
}  // namespace life::rules
}  // namespace mugen::sim
