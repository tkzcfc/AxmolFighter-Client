#pragma once

#include <cstdint>

namespace gameui
{

// 战斗模式接口。重写期间只保留本地模式骨架，不再持有世界。
class IBattleMode
{
public:
    virtual ~IBattleMode() = default;

    virtual bool init()                                = 0;
    virtual void onUpdate(float delta)                 = 0;
    virtual void setInput(uint32_t slot, bool pressed) = 0;
    virtual void onExit() {}
};

}  // namespace gameui
