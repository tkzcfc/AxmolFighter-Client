#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gameui
{

// 本地副本启动参数。
struct LocalBattleParams
{
    std::int32_t roomId         = 0;
    std::int32_t roleId         = 0;
    std::int32_t petId          = 0;
    std::int32_t autotestFrames = 0;
};

// 联网 1v1 决斗启动参数（来自 game 服 DuelStartPush）。
struct NetBattleParams
{
    std::uint32_t battleId         = 0;
    std::int32_t roomId            = 0;
    std::int64_t opponentPlayerId  = 0;
    std::string opponentName;
    // 全部参战者的角色配置 id（按槽位），用于预加载技能特效。
    std::vector<std::int32_t> roleIds;
};

}  // namespace gameui
