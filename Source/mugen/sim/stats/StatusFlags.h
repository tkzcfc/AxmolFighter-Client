#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/conf/GameDef.h"
#include <array>

namespace mugen::sim
{
// 控制限制状态的引用计数（沿用旧实现 ExtraState 的引用计数语义，枚举见 mugen::ExtraState）。
struct StatusFlags
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
    // 是否可接收控制。
    bool canControl() const { return !active(mugen::ExtraState::Stun) && !active(mugen::ExtraState::Frozen); }
    // 是否可移动。
    bool canMove() const
    {
        return canControl() && !active(mugen::ExtraState::Durance) && !active(mugen::ExtraState::Palsy);
    }
    MG_REFLECT(StatusFlags, m_refs)
};
}  // namespace mugen::sim
