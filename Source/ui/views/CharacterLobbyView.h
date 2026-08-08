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

private:
    void updateCharacterList();

    void onClickStartGameButton(EventContext* context);

    void onClickCharacterCreateItem(EventContext* context);

    void onClickGameoverButton(EventContext* context);

    long long m_selectedCharacterID = 0;
};

}  // namespace gameui
