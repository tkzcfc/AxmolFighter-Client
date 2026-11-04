#pragma once

#include "ui/widgets/common/Panel.h"
#include "mugen/conf/GameDef.h"

#include <array>

namespace gameui
{

class CharacterCreationPanel : public Panel
{
public:
    CharacterCreationPanel() {}

    virtual ~CharacterCreationPanel() {}

protected:
    virtual std::vector<std::string> getPackages() const override
    {
        return {"UI/Res_PartnerIcons", "UI/Screen_CharacterLobby"};
    }

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

private:
    GComponent* m_roleTypeSelector = nullptr;
    std::array<GButton*, mugen::CharacterClass::kCount> m_roleTypeButtons{};

    int32_t m_curRoleTypeIndex = 0;
};

}  // namespace gameui
