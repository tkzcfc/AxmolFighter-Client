#pragma once

#include "ui/widgets/common/Panel.h"

#include <vector>

namespace gameui
{

class SkillMainPanel : public Panel
{
public:
    SkillMainPanel() = default;

protected:
    std::vector<std::string> getPackages() const override
    {
        return {"UI/Common_Base", "UI/Res_SkillIcons", "UI/Panel_Skill"};
    }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Panel_Skill", "SkillMainPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickBack(EventContext* context);
    void onClickConfigSlots(EventContext* context);
    void onClickItem(int32_t leafId);

    GComponent* container() const;
    void rebuildList();
    void refreshDetail();
    void refreshSelectionHighlights();
    void bindItem(GComponent* item, int32_t leafId, bool selected);
    int32_t occupation() const;
    std::string iconUrlForLeaf(int32_t leafId) const;

    GList* m_itemList        = nullptr;
    GTextField* m_titleText  = nullptr;
    GTextField* m_detailText = nullptr;
    GButton* m_btnConfig     = nullptr;

    int32_t m_selectedLeafId = 0;
    std::vector<int32_t> m_listIds;
};

}  // namespace gameui
