#pragma once

#include "mugen/sim_legacy/World.h"
#include "mugen/sim_legacy/input/FrameInput.h"
#include "mugen/core/math/FixedVec3.h"
#include "mugen/render/battle/WorldPresenter.h"
#include "net/NetAgent.h"
#include "net/client_game.pb.h"
#include "net/client_town.pb.h"
#include "net/game_types.pb.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace gameui
{

// 城镇远端玩家同步器：
// - 维护每个远端玩家的服务端状态快照缓冲（以 server_time_ms 为键）；
// - 估计服务端时钟偏移，按「估计服务端现在 - 插值延迟」采样并线性插值；
// - 把插值结果喂给现有的木偶更新路径，并把服务端下发的 TownAppearance / pet_id 应用到表现层。
class TownSync
{
public:
    TownSync(mugen::sim_legacy::World& world, mugen::WorldPresenter& presenter, net::NetAgent& agent);

    // 注册城镇推送监听（enter/leave/appearance/states）。在 Agent 生命周期内调用一次。
    void start();

    // 用进入场景返回的玩家快照生成全部远端玩家。
    void applyEnterPlayers(const PB::Game::EnterSceneResp& resp);

    // 每个逻辑帧调用一次：推进插值并产出木偶更新。
    void tick(float logicDt);

    // 取走并清空本逻辑帧累积的木偶更新。
    std::vector<mugen::sim_legacy::PuppetUpdate> takePuppetUpdates();

    // 清空全部远端玩家（含宠物）。
    void clear();

    void setLocalPlayerId(int64_t id) { m_localPlayerId = id; }

    // 远端玩家摘要（供调试面板列出并发起决斗邀请）。
    struct RemotePlayerInfo
    {
        int64_t playerId = 0;
        std::string name;
        int32_t classId = 0;
        int32_t level   = 0;
    };

    // 按 player_id 升序返回当前场景内的远端玩家。
    std::vector<RemotePlayerInfo> remotePlayers() const;

private:
    // 单个服务端状态快照（线路定点已转成像素浮点，仅供表现层插值）。
    struct Snapshot
    {
        uint64_t serverTimeMs = 0;
        float x               = 0.0f;
        float y               = 0.0f;
        float z               = 0.0f;
        float vx              = 0.0f;
        float vy              = 0.0f;
        float vz              = 0.0f;
        int32_t towardX       = 1;
        bool moving           = false;
        std::string motionName;
    };

    struct RemotePlayer
    {
        mugen::Entity entity;
        std::map<uint64_t, Snapshot> snapshots;  // 以 server_time_ms 为键
        std::string name;
        int32_t classId = 0;
        int32_t level   = 0;
    };

    static void rememberAppearance(RemotePlayer& player, const PB::Types::TownAppearance& appearance);

    void addRemotePlayer(const PB::Types::TownPlayerInfo& info);
    void removeRemotePlayer(int64_t playerId);
    void applyAppearance(mugen::Entity entity, const PB::Types::TownAppearance& appearance);
    void pushSnapshot(int64_t playerId, uint64_t serverTimeMs, const PB::Types::TownMovement& movement);
    void updateClockOffset(uint64_t serverTimeMs);

    // 估计服务端当前时间（本地单调时钟 + 平滑偏移）。
    uint64_t estimatedServerNowMs() const;

    // 在指定渲染时刻采样某玩家状态；缓冲为空返回 false。
    bool sampleRemote(const RemotePlayer& player, uint64_t renderTimeMs, Snapshot* out) const;

    static mugen::FixedVec3 toFixedVec3(float x, float y, float z);

    mugen::sim_legacy::World& m_world;
    mugen::WorldPresenter& m_presenter;
    net::NetAgent& m_agent;
    int64_t m_localPlayerId = 0;

    // 服务端时钟偏移（ms，= 服务端时间 - 本地单调时钟），EMA 平滑。
    double m_clockOffsetMs = 0.0;
    bool m_hasOffset       = false;

    std::map<int64_t, RemotePlayer> m_players;
    std::vector<mugen::sim_legacy::PuppetUpdate> m_pendingPuppetUpdates;
};

}  // namespace gameui
