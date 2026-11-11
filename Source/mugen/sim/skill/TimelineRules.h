#pragma once
#include "mugen/core/ecs/Entity.h"
#include <cstdint>

namespace mugen::sim
{
class World;
namespace timeline::rules
{
// 进入指定片段：播放动作、准入片段 Buff、覆盖位移。
// effect 为 true 时读取特效片段表。deferAdvance 为 true 时，下一次时间轴遍历只消费该标记不推进。
// 配置缺失返回 false。
bool enterClip(World& world, Entity entity, int32_t clipId, bool effect, bool deferAdvance);
// 退出当前片段：移除片段 Buff。没有当前片段时直接返回。
void exitClip(World& world, Entity entity);
// 推进当前片段时间并触发到点的 cue。片段完成（动画完成且尾延时结束）时返回 true。
bool advanceClip(World& world, Entity entity);
}  // namespace timeline::rules
}  // namespace mugen::sim
