#pragma once
#include "mugen/sim/buff/BuffComponents.h"
#include "mugen/conf/battle/BuffDef.h"

namespace mugen::sim
{
class World;
namespace buff::rules
{
// 准入。context 默认时施加者就是接收者，没有事件目标。
void admit(World& world, Entity entity, int32_t buffId, const BuffContext& context = {});
// 移除该 id 的全部实例。逻辑立即失效。
void remove(World& world, Entity entity, int32_t buffId);
// 按规则移除一层或全部层。
void removeRule(World& world, Entity entity, int32_t ruleId, bool all);
// 同步触发。新实例不接收正在进行的这一次分派。
void fire(World& world, Entity entity, conf::battle::TriggerPoint point, const BuffContext& context = {});
// 召唤物死亡：主人身上的链接提前失效。
void notifySummonDeath(World& world, Entity owner);
// P2：周期、冷却、监视、特殊能力计时。
void tick(World& world);
}  // namespace buff::rules
}  // namespace mugen::sim
