#pragma once

#include <cstdint>

namespace gameui
{

struct PartnerInstanceState
{
    int32_t configId = 0;
    int32_t level    = 1;
    int32_t star     = 0;
    bool unlocked    = false;
};

}  // namespace gameui
