#pragma once

#include "ui/widgets/common/Panel.h"
#include "WeaponTypes.h"

#include <vector>

namespace gameui
{

// 武装主界面 (vertical slice): list all unlocked armaments, set appearance,
// configure skill, free 升星/升级/进阶. Local state only; FairyGUI Panel_Weapon
// package can replace programmatic layout later.
class WeaponMainPanel : public Panel
{
public:
    WeaponMainPanel() = default;

protected:
    std::vector<std::string> getPackages() const override { return {"UI/Common_Base"}; }

    GComponent* onCreatePanelContent() override;

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
    void onClickPrevPage(EventContext* context);
    void onClickNextPage(EventContext* context);

    void rebuildList();
    void refreshDetail();
    int32_t occupation() const;

    GList* m_itemList       = nullptr;
    GTextField* m_titleText = nullptr;
    GTextField* m_detailText = nullptr;
    GButton* m_btnAppearance = nullptr;
    GButton* m_btnSkill      = nullptr;
    GButton* m_btnLevel      = nullptr;
    GButton* m_btnStar       = nullptr;
    GButton* m_btnGrade      = nullptr;

    int32_t m_selectedId = 0;
    int32_t m_page = 0;
    static constexpr int kPageSize = 12;
    std::vector<int32_t> m_listIds;
    GButton* m_btnPrevPage = nullptr;
    GButton* m_btnNextPage = nullptr;
};

}  // namespace gameui
