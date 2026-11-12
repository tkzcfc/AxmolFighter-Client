#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/Fixed.h"
#include <array>
#include <cstdint>
#include <vector>

namespace mugen::sim
{
// 属性槽位：16 个基础属性（与旧实现 Attributes.basic 顺序一致）。
enum class StatId : int32_t
{
    SourceForce = 0,   // 力量
    Agility,           // 敏捷
    Habitus,           // 体质
    Spirit,            // 精神
    Hp,                // 生命
    Atk,               // 攻击
    Def,               // 防御
    Matk,              // 魔法攻击
    Mdef,              // 魔法防御
    Crit,              // 暴击
    CritResist,        // 抗暴击
    CritDamage,        // 暴击伤害
    CritDamageResist,  // 抗暴击伤害
    Dodge,             // 闪避
    Hit,               // 命中
    BaseDamage         // 基础伤害
};
// 基础属性数量。
inline constexpr int32_t kBaseStatCount = 16;
// 一条属性修正，按来源实例撤销。
struct StatMod
{
    // 属性号。基础 0–15，扩展 100 起，倍率 200 起，锁血 300。
    int32_t m_stat = 0;
    // 已求值的增量。
    Fixed m_value;
    // 来源实例序号。
    uint32_t m_source = 0;
    // 周期重算。
    bool m_live = false;
    MG_REFLECT(StatMod, m_stat, m_value, m_source, m_live)
};
// 角色基础、叠加和扩展属性（与旧实现 Attributes 一致）。
struct StatsComponent
{
    // 基础属性。
    std::array<Fixed, kBaseStatCount> m_base{};
    // 临时叠加值。
    std::array<Fixed, kBaseStatCount> m_overlay{};
    // 扩展属性（下标 = 扩展属性 id - 100）。
    std::array<Fixed, kBaseStatCount> m_extend{};
    // 基础属性、叠加值与修正之和。
    Fixed value(int32_t index) const
    {
        return m_base[static_cast<size_t>(index)] + m_overlay[static_cast<size_t>(index)] + sumMods(index);
    }
    // 扩展属性与叠加修正之和。
    Fixed extended(int32_t id) const { return m_extend[static_cast<size_t>(id - 100)] + sumMods(id); }
    // 生命上限（基础生命 + 扩展属性 106 + 两侧修正）。
    Fixed maxHp() const { return value(4) + extended(106); }
    // 倍率类基础为 1，治疗效率与锁血为基础 0，其余走基础槽。
    Fixed finalStat(int32_t stat) const
    {
        if (stat >= 0 && stat < kBaseStatCount)
            return value(stat);
        if (stat >= 100 && stat < 116)
            return extended(stat);
        if (stat == 208 || stat == 300)
            return sumMods(stat);
        if (stat >= 200 && stat <= 210)
            return Fixed::fromInt(1) + sumMods(stat);
        return sumMods(stat);
    }
    // 同一属性号的修正之和。
    Fixed sumMods(int32_t stat) const
    {
        Fixed total;
        for (const auto& mod : m_mods)
            if (mod.m_stat == stat)
                total += mod.m_value;
        return total;
    }
    // 按来源登记的修正。
    std::vector<StatMod> m_mods;
    MG_REFLECT(StatsComponent, m_base, m_overlay, m_extend, m_mods)
};
}  // namespace mugen::sim
