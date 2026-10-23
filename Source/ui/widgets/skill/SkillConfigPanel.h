#pragma once

#include "ui/widgets/common/Panel.h"
#include "mugen/conf/GameDef.h"

#include <vector>

namespace gameui
{

class SkillConfigPanel : public Panel
{
public:
    SkillConfigPanel() = default;

protected:
    std::vector<std::string> getPackages() const override
    {
        return {"UI/Common_Base", "UI/Res_SkillIcons", "UI/Panel_Skill"};
    }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Panel_Skill", "SkillConfigPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickBack(EventContext* context);
    void onClickClearSlot(EventContext* context);
    void onClickReset(EventContext* context);
    void onClickSlot(mugen::EnumSkillSlot slot);
    void onClickSkill(int32_t leafId);

    GComponent* container() const;
    void rebuildSlots();
    void rebuildSkills();
    void refreshSlotHighlights();
    void refreshSkillHighlights();
    void bindSlotItem(GComponent* item, mugen::EnumSkillSlot slot, const char* label, int32_t leafId,
                      bool selected);
    void bindSkillItem(GComponent* item, int32_t leafId, bool selected);
    int32_t occupation() const;
    std::string iconUrlForLeaf(int32_t leafId) const;

    GList* m_slotList       = nullptr;
    GList* m_skillList      = nullptr;
    GTextField* m_titleText = nullptr;
    GTextField* m_hintText  = nullptr;
    GButton* m_btnClear     = nullptr;
    GButton* m_btnReset     = nullptr;

    mugen::EnumSkillSlot m_selectedSlot = mugen::EnumSkillSlot::Attack;
    int32_t m_selectedLeafId            = 0;
};

}  // namespace gameui
