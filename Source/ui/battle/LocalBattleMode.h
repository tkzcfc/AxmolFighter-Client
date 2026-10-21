#pragma once

#include "ui/battle/BattleMode.h"
#include "ui/battle/BattleBootParams.h"
#include "mugen/sim/Replay.h"
#include "mugen/sim/World.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "ui/battle/BattleDebugPanel.h"

#include <cstdint>

namespace gameui
{

class LocalBattleMode : public IBattleMode
{
public:
    LocalBattleMode();
    explicit LocalBattleMode(int32_t roomId);
    explicit LocalBattleMode(LocalBattleParams params);
    ~LocalBattleMode() override;

    bool init() override;
    void onUpdate(float delta) override;
    void setInput(uint32_t slot, bool pressed) override;
    void onExit() override;
    void renderDebugPanel();
    // 切换碰撞框调试绘制，只影响表现层。
    void toggleCollisionDebug();

    void setReplayPath(std::string path) { m_replayPath = std::move(path); }
    mugen::sim::World& world() { return m_world; }
    const mugen::sim::World& world() const { return m_world; }

    int32_t roomId() const { return m_roomId; }
    int32_t autotestFrames() const { return m_autotestFrames; }
    // 按钮只提交下一帧系统命令。
    void requestReturnToTown() { m_systemCommands.push_back(mugen::sim::ReturnToTownCommand{}); }
    // HUD 暂停按钮复用调试暂停开关，只停止本地推进。
    void setPaused(bool paused) { m_debugPaused = paused; }
    // 最近一次受到伤害数字事件的非英雄角色，供 HUD 目标血条使用。
    mugen::Entity lastHurtTarget() const { return m_lastHurtTarget; }
    bool returnToTownRequested() const { return m_world.ecs().singleton<mugen::sim::RoomState>().m_returnToTown; }

private:
    int32_t m_roomId         = 0;
    int32_t m_roleId         = 0;
    int32_t m_autotestFrames = 0;
    mugen::sim::World m_world;
    mugen::sim::ControlManagerCore m_control;
    mugen::sim::Replay m_replay;
    std::string m_replayPath;
    float m_accumulator    = 0.0f;
    bool m_initialized     = false;
    bool m_debugPaused     = false;
    bool m_debugSingleStep = false;
    BattleDebugPanel m_debugPanel;
    // 待提交输入及最近的房间表现消息。
    std::vector<mugen::sim::SystemCommand> m_systemCommands;
    std::string m_roomMessage;
    mugen::Entity m_lastHurtTarget;
    class WorldPresenterHolder;
    std::unique_ptr<WorldPresenterHolder> m_presenter;
};

}  // namespace gameui
