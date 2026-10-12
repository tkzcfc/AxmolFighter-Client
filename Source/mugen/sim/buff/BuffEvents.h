#pragma once
#include "BuffComponents.h"
#include "mugen/conf/GameDef.h"

namespace mugen::sim { class World; }

namespace mugen::sim::buff
{
// 判断事件是否满足初始化事件表、目标过滤和绑定技能限制。
bool acceptsEvent(const conf::BuffConfig& current, const conf::BuffConfig& initial,
                  const BuffInstance& instance, Entity owner, BFEvent event,
                  const BuffContext& context, int32_t targetCamp);
// 按实例插入顺序同步派发事件，回调后重新定位实例。
void dispatchEvent(World& world, Entity entity, BFEvent event, const BuffContext& context, int32_t targetCamp);
// 行为状态只转发给状态施加器规则，具体规则保持未移植分派。
void dispatchBehaviorState(World& world, Entity entity, BFEvent event, const char* actionName);
}
