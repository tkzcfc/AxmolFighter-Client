#pragma once
#include "mugen/sim/stats/StatsComponents.h"
#include "mugen/conf/battle/HitDef.h"
#include "mugen/core/math/Fixed.h"
#include <algorithm>

namespace mugen::sim
{
namespace combat::rules
{
// 伤害公式输入。
struct DamageInput
{
    // 攻击方属性。
    const Stats* m_attacker;
    // 受击方属性。
    const Stats* m_target;
    // 命中配置。
    const conf::battle::HitDef* m_hit;
    // 已解析的标准伤害。
    Fixed m_standardHurt;
    // 已解析的非负等级加成（B2 无技能属性系统，恒为 0）。
    Fixed m_addition;
    // 是否 PVP（B2 恒 false）。
    bool m_pvp = false;
};
// 属性闪避判定（必中跳过；抽样 draw 返回 0-100）。
inline bool attributeDodge(const DamageInput& input, Fixed draw)
{
    if (input.m_hit->hitMust != 0)
        return false;
    const Fixed dodge    = input.m_target->value(13) + input.m_target->extended(109);
    const Fixed accuracy = input.m_attacker->value(14) + input.m_attacker->extended(111);
    const Fixed rate     = std::clamp((dodge - accuracy) / "100"_fx, "0"_fx, "0.5"_fx);
    return rate * "100"_fx >= draw;
}
// 按攻防、倍率、暴击、增减伤、浮动的顺序计算伤害（crit 与 float 各抽样一次）。
inline Fixed calculateDamage(const DamageInput& input, Fixed critDraw, Fixed floatDraw, bool& critical)
{
    const Stats& attack = *input.m_attacker;
    const Stats& target = *input.m_target;
    const auto& hit     = *input.m_hit;
    const bool physical = hit.hurtType == 0 || (hit.hurtType == 2 && attack.value(5) >= attack.value(7));
    Fixed atk           = attack.value(physical ? 5 : 7);
    Fixed def           = target.value(physical ? 6 : 8);
    const Fixed rate    = hit.hurtType == 3 ? "1"_fx : (atk + def == "0"_fx ? "0"_fx : atk / (atk + def));
    Fixed hurt          = input.m_standardHurt * rate;
    hurt *= hit.hurtRate * ("1"_fx + input.m_addition);
    const Fixed critRate =
        std::clamp((attack.value(9) + attack.extended(102) - target.value(10) - target.extended(103)) / "100"_fx,
                   "0.01"_fx, "1"_fx) *
        "100"_fx;
    const Fixed critMultiplier = std::clamp(
        "1.5"_fx + (attack.value(11) + attack.extended(112) - target.value(12) - target.extended(113)) / "100"_fx,
        "1.5"_fx, "2.5"_fx);
    critical = critRate >= critDraw;
    if (critical)
        hurt = critMultiplier * hurt;
    hurt *= "1"_fx + attack.extended(100);
    hurt *= "1"_fx - target.extended(101);
    hurt = hurt * ("0.02"_fx * floatDraw + "99"_fx) / "100"_fx;
    return std::max(hurt, "0"_fx).floor();
}
}  // namespace combat::rules
}  // namespace mugen::sim
