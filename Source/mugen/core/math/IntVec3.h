#pragma once

#include <cstdint>

#include "mugen/core/MacroDefinition.h"

NS_MG_BEGIN

// 用于离散逻辑坐标的三维 32 位整数向量。
struct IntVec3
{
    // X 轴分量。
    int32_t x = 0;
    // Y 轴分量。
    int32_t y = 0;
    // Z 轴分量。
    int32_t z = 0;
};

NS_MG_END
