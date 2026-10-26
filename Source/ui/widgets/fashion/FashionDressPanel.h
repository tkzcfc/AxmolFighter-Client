#pragma once

#include "ui/widgets/common/Panel.h"
#include "fairygui/GLoader3D.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/tables/FashionTables.h"
#include "mugen/avatar/render/Avatar.h"

#include <optional>
#include <unordered_map>
#include <vector>

namespace gameui
{

// Dress panel: suit whole-set wear / single-part override; Save writes local session fashion.
// No WearFashion protocol yet — Save only updates GameSessionModel (stub logged).
class FashionDressPanel : public Panel
{
public:
    // initialPart == nullopt → Suit tab; otherwise open that part tab.
    explicit FashionDressPanel(std::optional<mugen::FashionPosition> initialPart = std::nullopt);

protected:
    std::vector<std::string> getPackages() const override
    {
        return {"UI/Res_GeneralIcons", "UI/Res_FashionIcons", "UI/Res_FashionSuitIcons", "UI/Panel_Fashion"};
    }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Panel_Fashion", "FashionDressPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    struct TabEntry
    {
        std::optional<mugen::FashionPosition> part;  // nullopt = Suit
        const char* title = "";
    };

    void onClickBack(EventContext* context);
    void onClickSave(EventContext* context);
    void onClickWalk(EventContext* context);
    void onClickTab(int index);
    void onClickSuitItem(int32_t suitId);
    void onClickFashionItem(int32_t fashionId);

    GComponent* container() const;
    void loadDraftFromSession();
    void rebuildTabs();
    void configureItemListLayout();
    void rebuildItemList();
    void refreshSelectionHighlights();
    void refreshAvatar();
    void applySuit(int32_t suitId);
    void applyFashion(int32_t fashionId);
    void saveDraftToSession();
    int32_t occupation() const;
    int32_t roleId() const;
    const mugen::conf::FashionConfig* findFashion(int32_t fashionId) const;
    std::vector<const mugen::conf::FashionConfig*> fashionsForPart(mugen::FashionPosition part) const;
    std::vector<const mugen::conf::FashionSuitConfig*> suitsForOccupation() const;
    void bindFashionItem(GComponent* item, int32_t fashionId, bool selected);
    void bindSuitItem(GComponent* item, int32_t suitId, bool selected);

    std::optional<mugen::FashionPosition> m_initialPart;
    std::optional<mugen::FashionPosition> m_activePart;  // nullopt = Suit tab
    std::vector<TabEntry> m_tabs;
    std::unordered_map<mugen::FashionPosition, int32_t> m_draftEquip;  // FashionConfig.id
    int32_t m_selectedSuitId    = 0;
    int32_t m_selectedFashionId = 0;

    GList* m_tabList          = nullptr;
    GList* m_itemList         = nullptr;
    GLoader3D* m_avatarLoader = nullptr;
    mugen::Avatar* m_avatar   = nullptr;
    GTextField* m_titleText   = nullptr;
    GButton* m_saveButton     = nullptr;
    GButton* m_walkButton     = nullptr;
    bool m_walkPose           = false;
};

}  // namespace gameui
