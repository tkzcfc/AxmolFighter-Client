#pragma once

#include "ui/battle/BattleMode.h"

#include <cstdint>

namespace gameui
{

// 本地战斗骨架。世界已拆除，只记住房间 id，供返回城镇流程使用。
class LocalBattleMode : public IBattleMode
{
public:
    LocalBattleMode() = default;
    explicit LocalBattleMode(int32_t roomId);

    bool init() override;
    void onUpdate(float delta) override;
    void setInput(uint32_t slot, bool pressed) override;

    int32_t roomId() const { return m_roomId; }

private:
    // >0：副本房间 id；0：无房间的本地入口
    int32_t m_roomId = 0;
};

}  // namespace gameui
