#pragma once

#include "ui/battle/BattleMode.h"
#include "ui/battle/BattleBootParams.h"
#include "mugen/sim/World.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/sim/input/FrameInput.h"
#include "mugen/sim/present/PresentationEvent.h"
#include "mugen/sim/replay/Replay.h"
#include "mugen/sim/view/WorldView.h"
#include "mugen/sim/view/HudView.h"
#include "ui/battle/BattleDebugPanel.h"

#include <cstdint>
#include <optional>

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
    const mugen::sim::World& world() const override { return m_world; }
    mugen::Entity localPlayer() const override { return m_localPlayer; }

    int32_t roomId() const { return m_roomId; }
    int32_t autotestFrames() const { return m_autotestFrames; }
    // 按钮只提交下一帧系统命令。
    void requestReturnToTown() { m_systemCommands.push_back(mugen::sim::ReturnToTownCommand{}); }
    // HUD 暂停按钮复用调试暂停开关，只停止本地推进。
    void setPaused(bool paused) override { m_debugPaused = paused; }
    // 最近一次受到伤害数字事件的非英雄角色，供 HUD 目标血条使用。
    mugen::Entity lastHurtTarget() const override { return m_lastHurtTarget; }
    bool returnToTownRequested() const
    {
        return mugen::sim::view::readBattleRoom(m_world, m_localPlayer).returnToTown;
    }
    // 失败，或终点房胜利后才有值。普通房清完只开门，不产生结算。
    const DungeonBattleResult* result() const { return m_result ? &*m_result : nullptr; }

private:
    int32_t m_roomId         = 0;
    int32_t m_roleId         = 0;
    int32_t m_petId          = 0;
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
    // 本场结算，只写入一次。
    std::optional<DungeonBattleResult> m_result;
    mugen::Entity m_lastHurtTarget;
    void subscribeAppearanceEvents();
    // 记录一次结算。重复的胜负事件不再覆盖。
    void latchResult(bool win);
    void refreshLocalAppearance();
    int32_t resolveFollowPetId() const;
    void refreshFollowPet();

    class WorldPresenterHolder;
    std::unique_ptr<WorldPresenterHolder> m_presenter;
    mugen::Entity m_localPlayer{};
};

}  // namespace gameui
