#pragma once
#include "mugen/conf/tables/SkillTables.h"
#include "mugen/core/math/Random.h"
#include "mugen/sim/role/RoleComponents.h"
#include <algorithm>

namespace mugen::sim::hit
{
// 伤害公式的输入；标准伤害与等级加成由实体归属层解析。
struct DamageInput
{
    // 攻击方三层属性。
    Attributes m_attacker;
    // 受击方三层属性。
    Attributes m_target;
    // 本次打击配置；保留但不执行 PVP 倍率。
    conf::SkillHitConfig m_hit;
    // 已解析的标准伤害。
    Fixed m_standardHurt;
    // 已解析的非负等级加成。
    Fixed m_addition;
    // 神器或特殊能力来源使用神器增伤。
    bool m_artifact = false;
};
// 单次公式结果，不包含生命扣除或受击状态切换。
struct DamageResult
{
    // 向下取整后的非负伤害，不设最小伤害。
    Fixed m_damage;
    // 是否发生暴击。
    bool m_critical = false;
};
// 瞬时随机读取接口；不拥有随机状态，不存入 ECS。
struct DamageDraw
{
    // 外部随机状态或测试序列。
    void* m_context;
    // 返回零到一百范围内的下一次抽样。
    Fixed (*m_next)(void*);
    // 读取一次样本，不预取后续随机值。
    Fixed next() const { return m_next(m_context); }
};
// 将世界唯一 RNG 的引用适配到公式，不构造额外发生器。
inline DamageDraw damageDraw(Random& random)
{
    return {&random, [](void* context) -> Fixed {
        return static_cast<Random*>(context)->randomF("0"_fx, "100"_fx);
    }};
}
// 属性闪避先抽样；必中跳过抽样；零概率仍保留大于等于比较。
inline bool attributeDodge(const DamageInput& input, DamageDraw draw)
{
    if (input.m_hit.hitMust != 0) return false;
    const auto dodge = input.m_target.value(13) + input.m_target.extend[9];
    const auto accuracy = input.m_attacker.value(14) + input.m_attacker.extend[11];
    const auto rate = std::clamp((dodge - accuracy) / "100"_fx, "0"_fx, "0.5"_fx);
    return rate * "100"_fx >= draw.next();
}
// 按攻防、倍率、暴击、增减伤、浮动的顺序计算，抽样仅为暴击再浮动。
inline DamageResult calculateDamage(const DamageInput& input, DamageDraw draw)
{
    const auto& attack = input.m_attacker;
    const auto& target = input.m_target;
    const bool physical = input.m_hit.hurtType == 0 ||
        (input.m_hit.hurtType == 2 && attack.value(5) >= attack.value(7));
    const auto atk = attack.value(physical ? 5 : 7);
    const auto def = target.value(physical ? 6 : 8);
    const auto rate = input.m_hit.hurtType == 3 ? "1"_fx :
        (atk + def == "0"_fx ? "0"_fx : atk / (atk + def));
    auto hurt = input.m_standardHurt * rate;
    hurt *= input.m_hit.hurtRate * ("1"_fx + input.m_addition);
    const auto critRate = std::clamp(
        (attack.value(9) + attack.extend[2] - target.value(10) - target.extend[3]) / "100"_fx,
        "0.01"_fx, "1"_fx) * "100"_fx;
    const auto critMultiplier = std::clamp("1.5"_fx +
        (attack.value(11) + attack.extend[12] - target.value(12) - target.extend[13]) / "100"_fx,
        "1.5"_fx, "2.5"_fx);
    const bool critical = critRate >= draw.next();
    if (critical) hurt = critMultiplier * hurt;
    hurt *= "1"_fx + attack.extend[0];
    hurt *= "1"_fx - target.extend[1];
    if (input.m_artifact) hurt *= "1"_fx + attack.extend[14];
    hurt = hurt * ("0.02"_fx * draw.next() + "99"_fx) / "100"_fx;
    return {std::max(hurt, "0"_fx).floor(), critical};
}
}
