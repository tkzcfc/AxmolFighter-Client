#pragma once

#include "mugen/core/math/DamageBox.h"

#include <vector>

NS_MG_BEGIN

class Entity;
class ECSManager;
class Random;
class TransformComponent;
class SkillHitTableConfig;

// 命中公用：敌我、盒、结算。近战与特效 tryHit 共用。
namespace CombatHit
{

DamageBox makeRadiusAttackBox(const TransformComponent* tf, float radius);

bool boxesOverlap(const std::vector<DamageBox>& attackBoxes, const std::vector<DamageBox>& damageBoxes);

bool isHostile(Entity* a, Entity* b);

// hitTarget: -1 Both, 0 Enemy, 1 Friend
bool passHitTarget(int32_t hitTarget, bool hostile);

void applyHit(Entity* attacker,
              Entity* defender,
              Entity* effectEntity,
              const SkillHitTableConfig* hitTable,
              int32_t skillHitLookupId,
              Random& rng,
              ECSManager* ecs,
              const std::vector<int32_t>* extraControl);

// 敌方命中挂 debuff_id；友方碰撞挂 buff_all_id（owner 自己走 spawn 时的 buff_id）
void applyEffectContactBuffs(Entity* effectEntity, Entity* target, bool hostile, bool dodged);

}  // namespace CombatHit

NS_MG_END
