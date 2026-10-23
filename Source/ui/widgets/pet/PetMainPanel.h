#pragma once

#include "ui/widgets/common/Panel.h"
#include "mugen/conf/tables/PetTables.h"
#include "fairygui/GLoader.h"
#include "fairygui/GLoader3D.h"

#include <vector>

namespace gameui
{

class PetMainPanel : public Panel
{
public:
    PetMainPanel() = default;

protected:
    std::vector<std::string> getPackages() const override
    {
        return {"UI/Common_Base", "UI/Res_PetIcons", "UI/Panel_Pet"};
    }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Panel_Pet", "PetMainPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickBack(EventContext* context);
    void onClickActive(EventContext* context);
    void onClickLevel(EventContext* context);
    void onClickStar(EventContext* context);
    void onClickItem(int32_t petId);

    GComponent* container() const;
    void rebuildList();
    void refreshDetail();
    void refreshSpinePreview();
    void refreshSelectionHighlights();
    void bindItem(GComponent* item, int32_t petId, bool selected);
    const mugen::conf::PetConfig* findConfig(int32_t petId) const;

    GList* m_itemList        = nullptr;
    GTextField* m_titleText  = nullptr;
    GTextField* m_detailText = nullptr;
    GLoader3D* m_previewSpine = nullptr;
    GButton* m_btnActive     = nullptr;
    GButton* m_btnLevel      = nullptr;
    GButton* m_btnStar       = nullptr;

    int32_t m_selectedId     = 0;
    int32_t m_previewSpineId = 0;
    std::vector<int32_t> m_listIds;
};

}  // namespace gameui
