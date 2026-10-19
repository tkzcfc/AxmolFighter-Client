#pragma once
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim::behavior
{
// 受击叶子的值状态；角色状态仍只由状态位决定。
struct HitActionState
{
    // 动画结束或位移制动后的累计毫秒。
    Fixed m_time;
    // 当前打击行与位移行。
    int32_t m_hitId = -1, m_displacementId = -1;
    // 动画、位移以及本叶子的完成标记。
    bool m_animationEnd = false, m_displacementEnd = false, m_finished = false;
    // 挣脱是否已被本叶子观察。
    bool m_break = false;
    MG_REFLECT(HitActionState,
               m_time,
               m_hitId,
               m_displacementId,
               m_animationEnd,
               m_displacementEnd,
               m_finished,
               m_break)
};
}  // namespace mugen::sim::behavior
