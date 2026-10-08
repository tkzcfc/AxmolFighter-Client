#include "ui/battle/LocalBattleMode.h"

#include "axmol.h"

namespace gameui
{

LocalBattleMode::LocalBattleMode(int32_t roomId) : m_roomId(roomId) {}

bool LocalBattleMode::init()
{
    AXLOGI("LocalBattleMode: placeholder ready (room={})", m_roomId);
    return true;
}

void LocalBattleMode::onUpdate(float /*delta*/) {}

void LocalBattleMode::setInput(uint32_t /*slot*/, bool /*pressed*/) {}

}  // namespace gameui
