#pragma once

#include <cstdint>

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
};

}  // namespace gameui
