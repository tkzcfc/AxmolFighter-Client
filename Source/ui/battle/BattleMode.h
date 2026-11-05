#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/sim/World.h"

namespace gameui
{

// 联网战斗的延迟概况，供调试面板展示；本地战斗没有。
struct BattleNetInfo
{
    // 到战斗服务器的往返时延与抖动（毫秒，含至多一个服务器 tick 的持有时间）；0 表示尚无样本。
    int64_t m_rttMs = 0;
    int64_t m_jitterMs = 0;
    // 当前自适应输入领先量（帧）与逻辑帧时长（毫秒）。
    uint32_t m_leadFrames = 0;
    uint32_t m_frameMs = 33;
};

class IBattleMode
{
public:
    virtual ~IBattleMode() = default;

    virtual bool init()                                = 0;
    virtual void onUpdate(float delta)                 = 0;
    virtual void setInput(uint32_t slot, bool pressed) = 0;
    virtual void onExit() {}

    // 本地玩家实体（HUD 状态读取与镜头跟随）。
    virtual mugen::Entity localPlayer() const { return {}; }
    // 最近一次受到伤害数字的非英雄目标（HUD 目标血条），无则返回空实体。
    virtual mugen::Entity lastHurtTarget() const { return {}; }
    // 只读访问战斗世界（HUD 读取状态）。
    virtual const mugen::sim::World& world() const = 0;
    // 联网模式填充延迟概况并返回 true；本地模式返回 false。
    virtual bool netInfo(BattleNetInfo& /*out*/) const { return false; }
    // 暂停本地推进；仅本地模式支持，联网模式默认无操作。
    virtual void setPaused(bool /*paused*/) {}
};

}  // namespace gameui
