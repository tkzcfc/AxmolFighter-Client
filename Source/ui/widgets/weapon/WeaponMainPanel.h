#pragma once

#include "ui/widgets/common/Panel.h"
#include "WeaponTypes.h"
#include "mugen/conf/tables/ArmamentTables.h"
#include "fairygui/GLoader.h"
#include "fairygui/GLoader3D.h"

#include <vector>

namespace mugen
{
class MgSkeletonAnimation;
}

namespace gameui
{

// 武装主界面: icon list, armament Spine preview, free upgrade/star/grade,
// skill cycle, appearance, skill-video popup, form (Avatar) popup.
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
    void onClickSkillPreview(EventContext* context);
    void onClickFormPreview(EventContext* context);

    GComponent* container() const;
    void rebuildList();
    void refreshDetail();
    void refreshSelectionHighlights();
    void refreshSpinePreview();
    void bindItem(GComponent* item, int32_t armamentId, bool selected);
    int32_t occupation() const;
    const mugen::conf::ArmamentConfig* findConfig(int32_t armamentId) const;

    GList* m_itemList            = nullptr;
    GTextField* m_titleText      = nullptr;
    GTextField* m_detailText     = nullptr;
    GLoader3D* m_previewSpine    = nullptr;
    GButton* m_btnAppearance     = nullptr;
    GButton* m_btnSkill          = nullptr;
    GButton* m_btnLevel          = nullptr;
    GButton* m_btnStar           = nullptr;
    GButton* m_btnGrade          = nullptr;
    GButton* m_btnSkillPreview   = nullptr;
    GButton* m_btnFormPreview    = nullptr;

    int32_t m_selectedId      = 0;
    int32_t m_previewSpineId  = 0;
    std::vector<int32_t> m_listIds;
};

}  // namespace gameui
