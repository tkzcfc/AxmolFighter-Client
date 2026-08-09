#include "mugen/combat/DamageCalculator.h"

#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/TableConfig.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace DamageCalculator
{

namespace
{
float atkOf(const RoleAttributeConfig& a)
{
    return a.atk > 0.0f ? a.atk : a.physicalAttack;
}
float defOf(const RoleAttributeConfig& a)
{
    return a.def > 0.0f ? a.def : a.physicalDefense;
}
float matkOf(const RoleAttributeConfig& a)
{
    return a.matk > 0.0f ? a.matk : a.magicAttack;
}
float mdefOf(const RoleAttributeConfig& a)
{
    return a.mdef > 0.0f ? a.mdef : a.magicDefense;
}
}  // namespace

int32_t hurtStandardId(int32_t level, bool isHero)
{
    if (level <= 0)
        level = 1;
    if (isHero)
        return level;
    return (std::max)(1, (level + 9) / 10);
}

float calculateDamageRate(float atk, float def, float atkStandard)
{
    float rate = 1.0f;
    const float gap = atkStandard > 0.0f ? atkStandard : 1.0f;
    if (atk > def)
    {
        rate = 1.0f + (atk - def) / gap / 10.0f * 1.5f;
        if (rate > 2.5f)
            rate = 2.5f;
    }
    else if (atk < def)
    {
        if (def > 0.0f)
            rate = atk / def;
    }
    return rate;
}

float calculateCritRate(float crit, float critResist, float critStandard)
{
    float rate = 0.01f;
    const float gap = critStandard > 0.0f ? critStandard : 1.0f;
    if (crit > critResist)
    {
        rate = 0.05f + (crit - critResist) / gap / 10.0f * 0.45f;
        if (rate > 0.5f)
            rate = 0.5f;
    }
    else
    {
        rate = crit / (critResist + 0.01f) * 0.04f + 0.01f;
    }
    return rate;
}

float calculateCritDamageRate(float critDamage, float critDamageResist, float critDamageStandard)
{
    float rate = 1.5f;
    const float gap = critDamageStandard > 0.0f ? critDamageStandard : 1.0f;
    if (critDamage > critDamageResist)
    {
        rate = 1.5f + std::sqrt((critDamage - critDamageResist) / gap / 10.0f) * 1.5f;
        if (rate > 3.0f)
            rate = 3.0f;
    }
    return rate;
}

float calculateDodgeRate(float dodge, float hit, float dodgeStandard)
{
    float rate = 0.0f;
    const float gap = dodgeStandard > 0.0f ? dodgeStandard : 1.0f;
    if (dodge > hit)
        rate = 0.01f + std::pow((dodge - hit) / gap / 10.0f, 2.0f) * 0.74f;
    return rate;
}

DamageResult calculate(const DamageInput& in, Random& rng)
{
    DamageResult out;
    if (!in.attacker || !in.defender || !in.hitCfg)
        return out;

    const auto& atkAttr = in.attacker->currentAttribute;
    const auto& defAttr = in.defender->currentAttribute;

    // —— 闪避（hit_must 跳过）——
    if (!in.hitMust)
    {
        const float dodgeStd =
            in.hurtStd && in.hurtStd->dodgeStandard > 0 ? static_cast<float>(in.hurtStd->dodgeStandard) : 1.0f;
        float dodgeRate = calculateDodgeRate(defAttr.dodge, atkAttr.hit, dodgeStd);
        // ExtendAttribute 一期 0
        if (dodgeRate * 100.0f > rng.nextFloat(0.0f, 100.0f))
        {
            out.isDodge = true;
            out.damage  = 0.0f;
            return out;
        }
    }

    // —— hurt_type → atk/def ——
    int32_t flag = 1;
    const int32_t hurtType = in.hitCfg->hurtType;
    if (hurtType == 1)
        flag = 2;
    else if (hurtType == 2)
        flag = (atkOf(atkAttr) >= matkOf(atkAttr)) ? 1 : 2;
    else if (hurtType == 3)
        flag = 3;
    else
        flag = 0 == hurtType ? 1 : 1;

    float atk = 0.0f, def = 0.0f, rate = 1.0f;
    if (flag == 3)
    {
        rate = 1.0f;
    }
    else if (flag == 2)
    {
        atk  = matkOf(atkAttr);
        def  = mdefOf(defAttr);
        rate = calculateDamageRate(atk, def,
                                   in.hurtStd ? static_cast<float>(in.hurtStd->atkStandard) : 1.0f);
    }
    else
    {
        atk  = atkOf(atkAttr);
        def  = defOf(defAttr);
        rate = calculateDamageRate(atk, def,
                                   in.hurtStd ? static_cast<float>(in.hurtStd->atkStandard) : 1.0f);
    }

    const float basicHurt = atkAttr.baseDamage;
    float standHurt       = basicHurt;
    // 黑月 getStandardHurt(attacker)：仅英雄用攻击方等级 hurt；怪/召唤无主人时为 0（不用 monsterHurt）
    if (in.attackerHurtStd)
        standHurt += static_cast<float>(in.attackerHurtStd->hurt);

    float hurt = standHurt * rate;
    float hurtAddition = in.hitCfg->hurtRate > 0.0f ? in.hitCfg->hurtRate : 1.0f;
    const float addition = in.skillAddition >= 0.0f ? in.skillAddition : 0.0f;
    hurtAddition *= (1.0f + addition);
    hurt *= hurtAddition;

    // hurtScale：ExtendAttribute 一期 0 → 1
    float hurtScale = 1.0f;

    // —— 暴击 ——
    const float critStd =
        in.hurtStd && in.hurtStd->critStandard > 0 ? static_cast<float>(in.hurtStd->critStandard) : 1.0f;
    const float critDmgStd = in.hurtStd && in.hurtStd->critDamageStandard > 0
                                 ? static_cast<float>(in.hurtStd->critDamageStandard)
                                 : 1.0f;
    float critRate = 100.0f * calculateCritRate(atkAttr.crit, defAttr.critResist, critStd);
    float critDamageRate =
        calculateCritDamageRate(atkAttr.critDamage, 0.0f /*critDamageResist 一期*/, critDmgStd);

    if (critRate > rng.nextFloat(0.0f, 100.0f) || critRate > 100.0f)
    {
        hurt *= critDamageRate;
        out.isCrit = true;
    }

    // 0.99~1.01 浮动
    hurt *= (0.02f * rng.nextFloat(0.0f, 100.0f) + 99.0f) / 100.0f;
    if (hurtScale < 0.0f)
        hurtScale = 0.0f;
    hurt *= hurtScale;

    out.damage = (std::max)(0.0f, hurt);
    return out;
}

bool isInvincible(const Entity* /*entity*/)
{
    return false;
}

bool isSuperArmor(const Entity* /*entity*/)
{
    return false;
}

}  // namespace DamageCalculator

NS_MG_END
