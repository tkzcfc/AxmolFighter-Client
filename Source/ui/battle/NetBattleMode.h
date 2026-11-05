#pragma once

#include "ui/battle/BattleMode.h"
#include "ui/battle/BattleBootParams.h"
#include "ui/battle/BattleDebugPanel.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/sim/net/NetBattleSync.h"

#include <net/client_battle.pb.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace net
{
class NetAgent;
}

namespace gameui
{

// 联网 1v1 决斗战斗模式：服务器权威 lockstep + 客户端回滚（NetBattleSync）。
// 进入后先发 BattleReadyReq；收到 BattleStartPush 引导世界并启动本地预测，
// 随后以 BattleInputPush 提交预测输入、按 BattleFramesPush 确认帧对账，
// 哈希不一致时发 BattleResyncReq，收到 BattleSnapshotPush 复位。
class NetBattleMode : public IBattleMode
{
public:
    NetBattleMode(NetBattleParams params, net::NetAgent& agent);
    ~NetBattleMode() override;

    bool init() override;
    void onUpdate(float delta) override;
    void setInput(uint32_t slot, bool pressed) override;
    void onExit() override;
    void renderDebugPanel();

    const mugen::sim::World& world() const override;
    mugen::Entity localPlayer() const override { return m_localPlayer; }
    mugen::Entity lastHurtTarget() const override { return m_lastHurtTarget; }

    // 世界已引导且已就位（重连时需等快照复位完成），供 GameScreen 延迟打开 HUD。
    bool isStarted() const { return m_started && !m_awaitSnapshot; }
    // 战斗结束并确认返回后置位，供 GameScreen 切回城镇。
    bool returnToTownRequested() const { return m_returnToTown; }
    // 投降：发送 BattleLeaveReq。仅由玩家显式操作触发，关闭客户端 / 断线 / 切屏都不会调用。
    void requestLeave();

private:
    void registerPushes();
    void sendReady();
    void sendInputPush();
    void sendResync(uint32_t frame);
    void onBattleStart(const PB::Battle::BattleStartPush& push);
    void applyRemoteAppearances(const PB::Battle::BattleStartPush& push);
    void onFrames(const PB::Battle::BattleFramesPush& push);
    void onSnapshot(const PB::Battle::BattleSnapshotPush& push);
    void onEnd(const PB::Battle::BattleEndPush& push);
    void showResult(bool win, bool draw);
    void subscribeAppearanceEvents();
    void refreshLocalAppearance();
    int32_t resolveFollowPetId() const;

    class WorldPresenterHolder;
    std::unique_ptr<WorldPresenterHolder> m_presenter;
    std::unique_ptr<mugen::sim::net::NetBattleSync> m_sync;

    NetBattleParams m_params;
    net::NetAgent& m_agent;
    mugen::sim::ControlManagerCore m_control;
    BattleDebugPanel m_debugPanel;

    uint32_t m_localSlot = 0;
    uint32_t m_frameMs   = 33;
    bool m_initialized   = false;
    bool m_started       = false;
    // 已收到 resume 开始参数但快照尚未到达：此期间不推进、不对账。
    bool m_awaitSnapshot = false;
    bool m_ended         = false;
    bool m_leaveSent     = false;
    bool m_returnToTown  = false;
    // 渲染插值 alpha 的帧内进度累计。
    float m_renderAccum = 0.0f;
    mugen::Entity m_localPlayer{};
    mugen::Entity m_lastHurtTarget{};
};

}  // namespace gameui
