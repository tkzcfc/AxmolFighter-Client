#pragma once

#include "ui/core/View.h"

namespace gameui
{

class CharacterLobbyView : public View
{
public:
    std::vector<std::string> getPackages() const override { return {"UI/CharacterLobby"}; }

    GComponent* onCreateContent() override
    {
        return UIPackage::createObject("CharacterLobby", "CharacterLobbyView")->as<GComponent>();
    }

    virtual void onEnter() override;

    void requestCharacterList();
    void refreshCharacterList();

private:
    void updateCharacterList();

    void onClickStartGameButton(EventContext* context);

    void onClickCreatePlayerButton(EventContext* context);

    long long m_selectedCharacterID = 0;
};

}  // namespace gameui
