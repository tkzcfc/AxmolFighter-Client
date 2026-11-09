#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/math/Fixed.h"
#include <array>
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
// 角色基础、叠加和扩展属性（与旧实现 Attributes 一致）。
struct Stats
{
    // 基础属性。
    std::array<Fixed, kBaseStatCount> m_base{};
    // 临时叠加值。
    std::array<Fixed, kBaseStatCount> m_overlay{};
    // 扩展属性（下标 = 扩展属性 id - 100）。
    std::array<Fixed, kBaseStatCount> m_extend{};
    // 基础属性与叠加值之和。
    Fixed value(int32_t index) const
    {
        return m_base[static_cast<size_t>(index)] + m_overlay[static_cast<size_t>(index)];
    }
    // 按扩展属性 id 读取（100 AddHurt、101 AvoidHurt …）。
    Fixed extended(int32_t id) const { return m_extend[static_cast<size_t>(id - 100)]; }
    // 生命上限（基础生命 + 扩展属性 106 AddMaxHp）。
    Fixed maxHp() const { return m_base[4] + m_extend[6]; }
    MG_REFLECT(Stats, m_base, m_overlay, m_extend)
};
}  // namespace mugen::sim
