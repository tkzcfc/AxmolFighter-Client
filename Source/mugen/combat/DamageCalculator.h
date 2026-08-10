#pragma once

#include "mugen/core/StdC.h"

NS_MG_BEGIN

class AttributeComponent;
class SkillHitTableConfig;
class SkillHurtConfig;
class Random;
class Entity;

struct DamageResult
{
    float damage  = 0.0f;
    bool isCrit   = false;
    bool isDodge  = false;
};

struct DamageInput
{
    const AttributeComponent* attacker = nullptr;
    const AttributeComponent* defender = nullptr;
    const SkillHitTableConfig* hitCfg  = nullptr;
    const SkillHurtConfig* hurtStd     = nullptr;  // 受击方等级：攻防/暴击/闪避标准
    const SkillHurtConfig* attackerHurtStd = nullptr;  // 攻击方等级：standHurt（仅英雄 hurt）
    float skillAddition                = 0.0f;     // 技能等级加成，一期恒 0
    bool hitMust                       = false;
    bool isHeroDefender                = true;
};

/** SkillHurt 伤害公式 */
namespace DamageCalculator
{

/** 查 skill_hurt 标准表 id（英雄=等级；怪=floor((lv+9)/10)） */
int32_t hurtStandardId(int32_t level, bool isHero);

float calculateDamageRate(float atk, float def, float atkStandard);
float calculateCritRate(float crit, float critResist, float critStandard);
float calculateCritDamageRate(float critDamage, float critDamageResist, float critDamageStandard);
float calculateDodgeRate(float dodge, float hit, float dodgeStandard);

/**
 * 先闪避（hitMust 跳过），再算伤。
 * hurtScale = 1 + ADD_HURT(攻) - AVOID_HURT(防)。
 */
DamageResult calculate(const DamageInput& in, Random& rng);

/** Buff 引用计数：无敌 / 霸体 */
bool isInvincible(const Entity* entity);
bool isSuperArmor(const Entity* entity);

}  // namespace DamageCalculator

NS_MG_END
