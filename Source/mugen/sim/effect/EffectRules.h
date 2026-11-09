#pragma once
#include "mugen/sim/World.h"
#include "mugen/sim/Events.h"

namespace mugen::sim
{
namespace effect::rules
{
// 由主人动作生成特效：计算放置并追加 SpawnRequest。
// source 为技能来源上下文；hitEntity 为受击特效目标（可选）。
void spawnFromRole(World& world, Entity owner, int32_t effectId, const SourceContext& source);
// 生成受击特效（复用同配置同目标或新建）。
void spawnHitEffect(World& world, Entity owner, int32_t effectId, Entity hitEntity, const SourceContext& source);
}  // namespace effect::rules
}  // namespace mugen::sim
