#pragma once

#include "ui/battle/BattleMode.h"
#include "mugen/sim_legacy/World.h"

namespace gameui
{
class BattleDebugPanel
{
public:
    void render(mugen::sim_legacy::World& world, bool& paused, bool& singleStep, bool& collisionDebug);
    // 联网战斗的只读调试窗口：世界哈希、帧号、延迟（RTT）行与各角色位置。
    void renderNet(mugen::sim_legacy::World& world, const BattleNetInfo& net);
    // 绘制醒目的延迟行 `延迟(RTT): XX ms  抖动: XX ms` 及输入领先量；两个联网面板共用。
    static void renderNetLatency(const BattleNetInfo& net);
};
}  // namespace gameui
