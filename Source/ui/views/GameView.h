#pragma once

#include "ui/core/View.h"
#include "ui/battle/BattleBootParams.h"
#include "ui/battle/BattleMode.h"

#include <optional>

namespace mugen
{
class GameWord;
}

namespace gameui
{

class GameView : public View
{
public:
    typedef View Super;

public:
    GameView();
    explicit GameView(BattleBootParams boot);
    ~GameView() override;

    virtual void onEnter() override;
    virtual void onExit() override;
    virtual void onUpdate(float delta) override;

    // Keyboard
    void onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* event);
    void onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* event);

private:
    bool initGameWord();
    std::unique_ptr<IBattleMode> createBattleMode();
    void setInputFromKey(ax::EventKeyboard::KeyCode code, bool pressed);

private:
    std::optional<BattleBootParams> m_boot;
    std::unique_ptr<mugen::GameWord> m_gameWord;
    std::unique_ptr<IBattleMode> m_battleMode;
    std::map<ax::EventKeyboard::KeyCode, uint32_t> m_slotMap;
    ax::EventListenerKeyboard* m_keyboardListener = nullptr;
};

}  // namespace gameui
