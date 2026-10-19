#pragma once
#include "BuffComponents.h"
#include "mugen/conf/GameDef.h"

namespace mugen::sim::buff
{
// 判断事件是否满足初始化事件表、目标过滤和绑定技能限制。
bool acceptsEvent(const conf::BuffConfig& current,
                  const conf::BuffConfig& initial,
                  const BuffInstance& instance,
                  Entity owner,
                  BFEvent event,
                  const BuffContext& context,
                  int32_t targetCamp);
}  // namespace mugen::sim::buff
