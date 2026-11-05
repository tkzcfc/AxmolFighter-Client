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

// 联网战斗的一秒窗口统计，供调试面板区分「回滚 / 模拟」与「表现层」的耗时，不参与任何模拟逻辑。
struct NetBattlePerfWindow
{
    // 窗口内的渲染帧数。
    uint32_t renderFrames = 0;
    // 窗口内最长的渲染帧间隔（毫秒）。
    double deltaMsMax = 0.0;
    // 窗口内回滚次数、重推帧数、错误预测数（含本地槽位被服务器改写的次数）、预测停顿次数。
    uint64_t rollbacks            = 0;
    uint64_t resimFrames          = 0;
    uint64_t mispredictions       = 0;
    uint64_t localMispredictions  = 0;
    uint64_t stalls               = 0;
    // 单个渲染帧内（网络回调累计）重推帧数的最大值。
    uint32_t maxResimInFrame = 0;
    // 窗口内推进的逻辑帧数、追帧（单个渲染帧推进多于一帧）多出的帧数、单帧最大推进帧数。
    uint32_t tickedFrames  = 0;
    uint32_t catchUpFrames = 0;
    uint32_t maxTicksInFrame = 0;
    // 本地预测推进（快照 + 世界 tick）耗时：合计与单渲染帧最大值（毫秒）。
    double simMsTotal = 0.0;
    double simMsMax   = 0.0;
    // 网络回调（确认输入、回滚重推、哈希对账）耗时：合计与单次最大值（毫秒）。
    double netMsTotal = 0.0;
    double netMsMax   = 0.0;
    // 表现层 presentEvents 耗时：合计与单次最大值（毫秒）。
    double presentMsTotal = 0.0;
    double presentMsMax   = 0.0;
};

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
    // 把一次网络回调的耗时与重推帧数记入统计窗口。
    void recordNetCallback(double ms, uint64_t resimFrames);
    // 渲染帧末尾结算：折算单帧重推最大值、滚动一秒窗口。
    void finishRenderFrame(float delta);

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

    // 当前一秒窗口与上一个完整窗口（面板显示后者）。
    NetBattlePerfWindow m_perfWindow;
    NetBattlePerfWindow m_perfLast;
    // 整场战斗中出现过的最坏值，便于复现后读取。
    NetBattlePerfWindow m_perfWorst;
    // 当前窗口起点的回滚统计，窗口结束时求差。
    mugen::sim::rollback::RollbackStats m_perfStatsBase;
    float m_perfWindowElapsed = 0.0f;
    // 当前渲染帧内网络回调累计重推帧数。
    uint64_t m_frameResim = 0;
    // 本场战斗累计发出的重同步请求次数（哈希不一致时）。
    uint32_t m_resyncRequests = 0;
};

}  // namespace gameui
