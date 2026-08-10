#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

// 可行走/物理范围（由 .layer moveRange 汇总）
class MapScope : public Object
{
public:
    typedef Object Super;

public:
    int32_t x      = 0;
    int32_t y      = 0;
    int32_t width  = 0;
    int32_t height = 0;
    MG_DEFINE_SERIALIZABLE(x, y, width, height)
};

NS_MG_END
