#pragma once

#include "ui/battle/BattleBootParams.h"
#include "ui/battle/BattleMode.h"
#include "mugen/net/BattleNetSync.h"
#include "net/NetAgent.h"

#include <cstdint>
#include <memory>

namespace PB::Battle
{
class BattleSnapshotPush;
}

namespace gameui
{

// 联网战斗：本地预测 + snapshot 和解，输入上报 battle 服。
class OnlineBattleMode : public IBattleMode, public net::NetAgent
{
public:
    explicit OnlineBattleMode(BattleBootParams boot);
    ~OnlineBattleMode() override;

    bool init(mugen::GameWord* gameWord) override;
    void onUpdate(float delta) override;
    void setInput(uint32_t slot, bool pressed) override;
    void onExit() override;

private:
    void sendPendingInput();
    void onSnapshot(const PB::Battle::BattleSnapshotPush& push);

private:
    BattleBootParams m_boot;
    mugen::GameWord* m_gameWord = nullptr;
    std::unique_ptr<BattleNetSync> m_netSync;
    std::uint32_t m_inputMask = 0;
};

}  // namespace gameui
