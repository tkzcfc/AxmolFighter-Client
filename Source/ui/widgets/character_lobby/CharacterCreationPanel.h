#pragma once

#include "ui/core/UIWidget.h"
#include "ui/UIMediaPlayer.h"

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

    virtual void onDestroy() override;

    void updateUI();

    void onClickCreateButton(EventContext* context);

    void onClickBackButton(EventContext* context);

    void onClickRoleTypeButton(EventContext* context, int32_t index);

    void onClickProfessionButton(EventContext* context, int32_t index);

private:
    GComponent* m_roleTypeSelector = nullptr;

    int32_t m_curRoleTypeIndex   = 0;
    int32_t m_curProfessionIndex = 0;

    ax::ui::MediaPlayer* m_mediaPlayer = nullptr;
};

}  // namespace gameui
