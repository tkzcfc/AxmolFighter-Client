#pragma once

#include <cstdint>
#include <string>

namespace game::model
{

struct AccountModel
{
    int64_t playerID = 0;  // 账号ID
    std::string account;   // 账号名
    std::string nickname;  // 昵称

    bool isLoggedIn() const { return playerID > 0; }
};

}  // namespace game::model
