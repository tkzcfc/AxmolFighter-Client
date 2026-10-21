#pragma once

#include "ui/widgets/common/Panel.h"
#include "ui/UIMediaPlayer.h"

namespace gameui
{

class CharacterCreationPanel : public Panel
{
public:
    CharacterCreationPanel() {}

    virtual ~CharacterCreationPanel() {}

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/Screen_CharacterLobby"}; }

    virtual GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Screen_CharacterLobby", "CharacterCreationPanel")->as<GComponent>();
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
