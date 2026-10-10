#pragma once

#include "mugen/core/reflect/Reflect.h"

NS_MG_BEGIN

// 可行走/物理范围（由 .layer moveRange 汇总）
struct MapScope
{
    // 范围左下角横坐标。
    int32_t x      = 0;
    // 范围左下角纵坐标。
    int32_t y      = 0;
    // 范围宽度。
    int32_t width  = 0;
    // 范围高度。
    int32_t height = 0;
    MG_REFLECT(MapScope, x, y, width, height)
};

NS_MG_END
