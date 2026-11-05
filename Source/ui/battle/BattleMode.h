#pragma once

#include "mugen/core/ecs/Entity.h"
#include "mugen/sim/World.h"

namespace gameui
{

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
    // 暂停本地推进；仅本地模式支持，联网模式默认无操作。
    virtual void setPaused(bool /*paused*/) {}
};

}  // namespace gameui
