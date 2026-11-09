#pragma once
#include "mugen/sim/WorldSetup.h"

namespace mugen::sim
{
// 把逻辑帧数换算为毫秒（定点，使用固定帧长）。
inline Fixed framesToMs(uint64_t frames)
{
    return kLogicDtMs * Fixed::fromInt(static_cast<int32_t>(frames));
}
}  // namespace mugen::sim
