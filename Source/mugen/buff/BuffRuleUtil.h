#pragma once

#include "mugen/buff/BuffRuleBase.h"
#include "mugen/buff/ExtendAttribute.h"
#include "mugen/core/StdC.h"

NS_MG_BEGIN

class Entity;
class BuffInstance;
class BuffConfig;
class AttributeComponent;
class SkillDeckEntry;

namespace BuffRuleUtil
{

float param(const BuffConfig* cfg, size_t index, float fallback = 0.0f);

/** binding / probability / innerCd；失败返回 false */
bool passTriggerGates(Entity* entity, BuffInstance& inst, const BuffConfig* cfg, int32_t skillId);

void modifyExtend(Entity* entity, ExtendAttributeType type, float delta);

SkillDeckEntry* findDeckEntry(Entity* entity, int32_t skillAttackId);

/** 挂/卸 Buff Spine（最小：跟随实体 + ResSpine） */
void attachSpine(Entity* entity, BuffInstance& inst);
void detachSpine(Entity* entity, BuffInstance& inst);

}  // namespace BuffRuleUtil

NS_MG_END
