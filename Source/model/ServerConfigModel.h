#pragma once

#include <cstdint>

namespace game::model
{

struct ServerConfigModel
{
    // 账号最多能创建多少个角色
    int32_t maxCharacterCount = 10;
};

}  // namespace game::model
