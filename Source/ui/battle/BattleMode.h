#pragma once

#include <cstdint>

namespace mugen
{
class GameWord;
}

namespace gameui
{

// 战斗模式接口：GameView 负责按键映射与事件转发，具体本地/联网行为由实现类完成。
class IBattleMode
{
public:
    virtual ~IBattleMode() = default;

    virtual bool init(mugen::GameWord* gameWord) = 0;
    virtual void onUpdate(float delta)           = 0;
    // 由 GameView 将按键映射为输入槽位后直接下发
    virtual void setInput(uint32_t slot, bool pressed) = 0;
    virtual void onExit() {}
};

}  // namespace gameui
