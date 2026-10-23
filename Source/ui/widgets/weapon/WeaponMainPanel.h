#pragma once

#include "ui/widgets/common/Panel.h"
#include "WeaponTypes.h"
#include "mugen/conf/tables/ArmamentTables.h"
#include "fairygui/GLoader.h"

#include <vector>

namespace gameui
{

// 武装主界面: list all unlocked armaments with icons, set appearance,
// configure skill, free 升星/升级/进阶. Local cheat state only.
class WeaponMainPanel : public Panel
{
public:
    WeaponMainPanel() = default;

protected:
    std::vector<std::string> getPackages() const override
    {
        return {"UI/Common_Base", "UI/Res_WeaponIcons", "UI/Res_WeaponSkillIcons", "UI/Panel_Weapon"};
    }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Panel_Weapon", "WeaponMainPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickBack(EventContext* context);
    void onClickItem(int32_t armamentId);
    void onClickAppearance(EventContext* context);
    void onClickSkill(EventContext* context);
    void onClickLevelUp(EventContext* context);
    void onClickStarUp(EventContext* context);
    void onClickGradeUp(EventContext* context);

    GComponent* container() const;
    void rebuildList();
    void refreshDetail();
    void refreshSelectionHighlights();
    void bindItem(GComponent* item, int32_t armamentId, bool selected);
    int32_t occupation() const;
    const mugen::conf::ArmamentConfig* findConfig(int32_t armamentId) const;

    GList* m_itemList          = nullptr;
    GTextField* m_titleText    = nullptr;
    GTextField* m_detailText   = nullptr;
    GLoader* m_previewIcon     = nullptr;
    GButton* m_btnAppearance   = nullptr;
    GButton* m_btnSkill        = nullptr;
    GButton* m_btnLevel        = nullptr;
    GButton* m_btnStar         = nullptr;
    GButton* m_btnGrade        = nullptr;

    int32_t m_selectedId = 0;
    std::vector<int32_t> m_listIds;
};

}  // namespace gameui
