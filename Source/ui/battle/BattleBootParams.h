#pragma once

#include <cstdint>
#include <string>

namespace gameui
{

// 联网战斗启动参数（决斗等入口传入 GameView）。
struct BattleBootParams
{
    std::uint32_t battleId      = 0;
    std::uint32_t serverFrame   = 0;
    std::uint32_t actorEntityId = 0;
    std::int32_t mapId          = 1;
    std::uint64_t randomSeed    = 0;
    std::string worldDump;
};

// 本地副本启动参数（单机调试期；联网后由 BattleBootParams 取代）
struct LocalBattleParams
{
    std::int32_t roomId = 0;  // RoomConfig id
};

}  // namespace gameui
