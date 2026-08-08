#pragma once

#include "ui/battle/BattleMode.h"

#include <cstdint>

namespace mugen
{
class GameWord;
}

namespace gameui
{

// 本地单机战斗：完整 Mugen 逻辑，无网络同步。
// roomId > 0 时按 RoomConfig 加载副本房间（kBattle）；否则加载城镇调试图。
class LocalBattleMode : public IBattleMode
{
public:
    LocalBattleMode() = default;
    explicit LocalBattleMode(int32_t roomId);

    bool init(mugen::GameWord* gameWord) override;
    void onUpdate(float delta) override;
    void setInput(uint32_t slot, bool pressed) override;

private:
    bool spawnLocalPlayer();

private:
    mugen::GameWord* m_gameWord = nullptr;
    // >0：副本房间 id；0：城镇调试图
    int32_t m_roomId = 0;
};

}  // namespace gameui
