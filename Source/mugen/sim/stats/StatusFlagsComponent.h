#pragma once
#include "mugen/conf/battle/BuffDef.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"
#include <algorithm>
#include <array>

namespace mugen::sim
{
// 控制限制状态的引用计数（沿用旧实现 ExtraState 的引用计数语义，枚举见 mugen::ExtraState）。
struct StatusFlagsComponent
{
    // 各限制状态的引用计数，下标 = ExtraState 值 - 1。
    std::array<int32_t, 8> m_refs{};
    // 判断限制状态是否生效。
    bool active(mugen::ExtraState state) const
    {
        const auto index = static_cast<int32_t>(state) - 1;
        return index >= 0 && index < static_cast<int32_t>(m_refs.size()) && m_refs[static_cast<size_t>(index)] > 0;
    }
    // 限制状态引用计数 +1。
    void add(mugen::ExtraState state)
    {
        const auto index = static_cast<int32_t>(state) - 1;
        if (index >= 0 && index < static_cast<int32_t>(m_refs.size()))
            ++m_refs[static_cast<size_t>(index)];
    }
    // 限制状态引用计数 -1（不越界）。
    void remove(mugen::ExtraState state)
    {
        const auto index = static_cast<int32_t>(state) - 1;
        if (index >= 0 && index < static_cast<int32_t>(m_refs.size()) && m_refs[static_cast<size_t>(index)] > 0)
            --m_refs[static_cast<size_t>(index)];
    }
    // Buff 标记引用 +1。禁疗和免晶体不在 ExtraState 里。
    void addFlag(mugen::conf::battle::BuffFlag flag)
    {
        if (flag == mugen::conf::battle::BuffFlag::NoHeal)
            ++m_noHeal;
        else if (flag == mugen::conf::battle::BuffFlag::NoCrystalCost)
            ++m_noCrystal;
        else if (flag >= mugen::conf::battle::BuffFlag::Durance && flag <= mugen::conf::battle::BuffFlag::Frozen)
            add(static_cast<mugen::ExtraState>(flag));
    }
    // Buff 标记引用 -1。
    void removeFlag(mugen::conf::battle::BuffFlag flag)
    {
        if (flag == mugen::conf::battle::BuffFlag::NoHeal)
            m_noHeal = std::max(0, m_noHeal - 1);
        else if (flag == mugen::conf::battle::BuffFlag::NoCrystalCost)
            m_noCrystal = std::max(0, m_noCrystal - 1);
        else if (flag >= mugen::conf::battle::BuffFlag::Durance && flag <= mugen::conf::battle::BuffFlag::Frozen)
            remove(static_cast<mugen::ExtraState>(flag));
    }
    // 是否可接收控制。
    bool canControl() const { return !active(mugen::ExtraState::Stun) && !active(mugen::ExtraState::Frozen); }
    // 是否可移动。
    bool canMove() const
    {
        return canControl() && !active(mugen::ExtraState::Durance) && !active(mugen::ExtraState::Palsy);
    }
    // 禁疗引用计数。
    int32_t m_noHeal = 0;
    // 免晶体消耗引用计数。
    int32_t m_noCrystal = 0;
    MG_REFLECT(StatusFlagsComponent, m_refs, m_noHeal, m_noCrystal)
};
}  // namespace mugen::sim
