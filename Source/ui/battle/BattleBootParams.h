#pragma once

#include <cstdint>

namespace gameui
{

// 本地副本启动参数。联网战斗入口在重写期间关闭。
struct LocalBattleParams
{
    std::int32_t roomId = 0;  // RoomConfig id
};

}  // namespace gameui
