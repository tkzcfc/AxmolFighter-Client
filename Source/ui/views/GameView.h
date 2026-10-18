#pragma once

#include "ui/core/View.h"
#include "ui/battle/BattleBootParams.h"
#include "ui/battle/BattleMode.h"

#include <map>
#include <optional>

namespace gameui
{

class GameView : public View
{
public:
    typedef View Super;

public:
    GameView();
    explicit GameView(LocalBattleParams local);
    ~GameView() override;

    virtual void onEnter() override;
    virtual void onExit() override;
    void onUpdate(float delta) override;
    virtual void onImGUIRender() override;

    void onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* event);
    void onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* event);

private:
    void onPrepareLoad() override;
    bool initInput();
    std::unique_ptr<IBattleMode> createBattleMode();
    void setInputFromKey(ax::EventKeyboard::KeyCode code, bool pressed);

private:
    std::optional<LocalBattleParams> m_local;
    std::unique_ptr<IBattleMode> m_battleMode;
    std::map<ax::EventKeyboard::KeyCode, uint32_t> m_slotMap;
    ax::EventListenerKeyboard* m_keyboardListener = nullptr;
};

}  // namespace gameui
