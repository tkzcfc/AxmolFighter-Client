#pragma once
#include "mugen/sim/World.h"
#include "mugen/sim/Events.h"

namespace mugen::sim
{
namespace effect::rules
{
// 按特效配置把 entity 放到 base 的相对位置；ground 为 true 时贴地（y=0）。
void placeFrom(World& world, Entity entity, Entity base, bool ground);
// 由主人动作生成特效：计算放置并追加 SpawnRequest。
// source 为技能来源上下文；hitEntity 为受击特效目标（可选）。
void spawnFromRole(World& world, Entity owner, int32_t effectId, const SourceContext& source);
// 后继特效：主人与技能来源沿用父特效，放置以父特效当前位置为基准。
void spawnChild(World& world, Entity parent, int32_t effectId);
// 生成受击特效。同目标已有实例时改放到新接触点并重播动作，否则新建。
void spawnHitEffect(World& world, Entity owner, Entity sourceEffect, int32_t effectId, Entity hitEntity,
                    const SourceContext& source, const ContactEvent& contact);
}  // namespace effect::rules
}  // namespace mugen::sim
