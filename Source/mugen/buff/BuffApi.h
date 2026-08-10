#pragma once

#include "mugen/buff/BFEvent.h"
#include "mugen/core/StdC.h"

NS_MG_BEGIN

class Entity;
class BuffInstance;
class BuffRuleBase;

namespace BuffApi
{

/** 添加 buff（叠层规则见 BuffSystem）；返回是否成功 */
bool addBuff(Entity* entity, int32_t buffId, int32_t sourceSkillId = 0, int32_t level = 1);

/** 移除指定 buffId 的实例（受 removeRepeatAll 影响） */
void removeBuff(Entity* entity, int32_t buffId);

/** 对实体身上所有 buff 广播事件 */
void trigger(Entity* entity, BFEvent event, Entity* other = nullptr, int32_t skillId = 0, float param = 0.0f);

BuffRuleBase* resolveRule(const BuffInstance& inst);

}  // namespace BuffApi

NS_MG_END
