#pragma once

#include "mugen/buff/BuffRuleBase.h"
#include "mugen/buff/ExtendAttribute.h"
#include "mugen/core/StdC.h"

#include <vector>

NS_MG_BEGIN

class Entity;
class Buff;
class BuffConfig;
class AttributeComponent;
class Skill;

namespace BuffRuleUtil
{

float param(const BuffConfig* cfg, size_t index, float fallback = 0.0f);

/** 表 interval/times 多为秒；>=100 视为已是毫秒 */
int32_t intervalMs(const BuffConfig* cfg);
int32_t durationMs(const BuffConfig* cfg);

/** binding / probability / innerCd；失败返回 false */
bool passTriggerGates(Entity* entity, Buff& inst, const BuffConfig* cfg, int32_t skillId);

void modifyExtend(Entity* entity, ExtendAttributeType type, float delta);

Skill* findSkill(Entity* entity, int32_t skillAttackId);

void modifyMp(Entity* entity, float delta);
void applyHpDelta(Entity* entity, float delta);

/** 1 GetUp / 2 HitRepel / 3 HitFly；其它 0 */
int32_t mappedBehaviorState(int32_t behaviorKind);
void notifyBehaviorKindChange(Entity* entity, int32_t oldKind, int32_t newKind);

/** target=Self 加到 holder；TargetEnemy 加到 other */
void addBuffToTarget(Entity* holder, Entity* other, int32_t buffId, int32_t sourceSkillId);
void addBuffIds(Entity* target, const std::vector<int32_t>& ids, int32_t sourceSkillId);

/** 挂/卸 Buff Spine（最小：跟随实体 + ResSpine） */
void attachSpine(Entity* entity, Buff& inst);
void detachSpine(Entity* entity, Buff& inst);

}  // namespace BuffRuleUtil

NS_MG_END
