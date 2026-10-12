#pragma once
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim::ai
{
// 巡逻、追击和警戒共用的可快照移动叶状态。
struct ActionMove
{
    // 到达目标或边界后开始停留。
    bool m_complete = false;
    // 本次进入抽取的停留时间及当前动作计时。
    Fixed m_delayMs, m_timeMs;
    MG_REFLECT(ActionMove, m_complete, m_delayMs, m_timeMs)
};
}
