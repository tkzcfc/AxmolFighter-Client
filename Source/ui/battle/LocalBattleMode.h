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
class LocalBattleMode : public IBattleMode
{
public:
    bool init(mugen::GameWord* gameWord) override;
    void onUpdate(float delta) override;
    void setInput(uint32_t slot, bool pressed) override;

private:
    bool spawnLocalPlayer();

private:
    mugen::GameWord* m_gameWord = nullptr;
};

}  // namespace gameui
