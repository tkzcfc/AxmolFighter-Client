#pragma once

#include "ui/core/Screen.h"

namespace gameui
{

class CharacterLobbyScreen : public Screen
{
public:
    std::vector<std::string> getPackages() const override { return {"UI/Screen_CharacterLobby"}; }

    GComponent* onCreateContent() override
    {
        return UIPackage::createObject("Screen_CharacterLobby", "CharacterLobbyScreen")->as<GComponent>();
    }

    virtual void onEnter() override;

    void requestCharacterList();
    void refreshCharacterList();

private:
    void updateCharacterList();

    void onClickStartGameButton(EventContext* context);

    void onClickCreatePlayerButton(EventContext* context);

    long long m_selectedCharacterID = 0;

    bool m_isWillEnterGame = false;

    GButton* m_startGameBtn = nullptr;
};

}  // namespace gameui
