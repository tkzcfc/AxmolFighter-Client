#include "doctest.h"
#include "mugen/sim/hit/Damage.h"
#include <array>

namespace
{
using namespace mugen;
using namespace mugen::sim;
// 固定顺序的抽样，供暴击与浮动两次读取。
struct Sequence
{
    std::array<Fixed, 2> m_values{};
    size_t m_cursor = 0;
    hit::DamageDraw draw()
    {
        return {this, [](void* context) -> Fixed {
            auto& sequence = *static_cast<Sequence*>(context);
            REQUIRE(sequence.m_cursor < sequence.m_values.size());
            return sequence.m_values[sequence.m_cursor++];
        }};
    }
};
// 构造不触发暴击且浮动倍率接近 1 的抽样序列。
Sequence neutralDraw()
{
    Sequence sequence;
    sequence.m_values = {Fixed::fromInt(100), Fixed::fromInt(50)};
    return sequence;
}
// 使用不触发暴击的抽样计算一次伤害。
Fixed runDamage(const hit::DamageInput& input)
{
    Sequence sequence = neutralDraw();
    return hit::calculateDamage(input, sequence.draw()).m_damage;
}
}  // namespace

// 竞技战斗改用 pvp_hurt_rate，普通战斗保持 hurt_rate。
TEST_CASE("Pvp damage uses pvp hurt rate instead of pve hurt rate")
{
    hit::DamageInput pve;
    pve.m_hit.hurtType        = 0;
    pve.m_attacker.basic[5]   = Fixed::fromInt(100);
    pve.m_target.basic[6]     = Fixed::fromInt(100);
    pve.m_standardHurt        = Fixed::fromInt(100);
    pve.m_hit.hurtRate        = "0.8"_fx;
    pve.m_hit.pvpHurtRate     = "1"_fx;
    pve.m_pvp                 = false;
    hit::DamageInput pvp = pve;
    pvp.m_pvp            = true;
    CHECK(runDamage(pve).toInt() == 40);
    CHECK(runDamage(pvp).toInt() == 50);
}

// 竞技战斗攻防缩放先向下取整再计算伤害倍率。
TEST_CASE("Pvp atk and def scale and floor before the damage rate")
{
    hit::DamageInput atkCase;
    atkCase.m_hit.hurtType       = 0;
    atkCase.m_attacker.basic[5]  = Fixed::fromInt(100);
    atkCase.m_target.basic[6]    = Fixed::fromInt(100);
    atkCase.m_attacker.m_pvpAtk  = "0.125"_fx;
    atkCase.m_standardHurt       = Fixed::fromInt(1000);
    atkCase.m_hit.hurtRate       = "1"_fx;
    atkCase.m_hit.pvpHurtRate    = "1"_fx;
    atkCase.m_pvp                = true;
    CHECK(runDamage(atkCase).toInt() == 528);

    hit::DamageInput defCase;
    defCase.m_hit.hurtType      = 0;
    defCase.m_attacker.basic[5] = Fixed::fromInt(100);
    defCase.m_target.basic[6]   = Fixed::fromInt(100);
    defCase.m_target.m_pvpDef   = "0.125"_fx;
    defCase.m_standardHurt      = Fixed::fromInt(1000);
    defCase.m_hit.hurtRate      = "1"_fx;
    defCase.m_hit.pvpHurtRate   = "1"_fx;
    defCase.m_pvp               = true;
    CHECK(runDamage(defCase).toInt() == 471);
}

// 真实伤害跳过攻防缩放，但倍率仍按竞技战斗选择。
TEST_CASE("True damage uses pvp rate but skips atk and def scaling")
{
    auto make = [](bool pvp) {
        hit::DamageInput input;
        input.m_hit.hurtType       = 3;
        input.m_attacker.basic[5]  = Fixed::fromInt(100);
        input.m_target.basic[6]    = Fixed::fromInt(100);
        input.m_attacker.m_pvpAtk  = "0.5"_fx;
        input.m_target.m_pvpDef    = "0.5"_fx;
        input.m_standardHurt       = Fixed::fromInt(100);
        input.m_hit.hurtRate       = "0.8"_fx;
        input.m_hit.pvpHurtRate    = "1"_fx;
        input.m_pvp                = pvp;
        return input;
    };
    CHECK(runDamage(make(false)).toInt() == 80);
    CHECK(runDamage(make(true)).toInt() == 100);
}
