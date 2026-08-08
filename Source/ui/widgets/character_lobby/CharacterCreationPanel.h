#pragma once

#include "ui/core/UIWidget.h"

namespace gameui
{

class CharacterCreationPanel : public UIWidget
{
public:
    CharacterCreationPanel()
    {
        m_options.hasBackground  = false;
        m_options.draggable      = false;
        m_options.fullscreen     = true;
        m_options.closeOnClickBg = false;
    }

    virtual ~CharacterCreationPanel() {}

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/CharacterLobby"}; }

    virtual GComponent* onCreateContent() override
    {
        return createCenteredComponent("CharacterLobby", "CharacterCreationPanel");
    }

    virtual void onCreate() override;

    void onClickCreateButton(EventContext* context);

    void onClickBackButton(EventContext* context);
};

}  // namespace gameui
