#include "SkillMainPanel.h"

#include "SkillLocalState.h"
#include "SkillConfigPanel.h"

#include "AppContext.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/tables/SkillTables.h"
#include "fairygui/UIPackage.h"

#include <fmt/format.h>

namespace gameui
{

namespace
{
const char* kItemUrl = "ui://Panel_Skill/SkillItem";

void setItemSelectedVisual(GComponent* item, bool selected)
{
    if (auto* btn = dynamic_cast<GButton*>(item))
        btn->setSelected(selected);
    if (auto* glow = item->getChild("loaderSelectGlow"))
        glow->setVisible(selected);
}
}  // namespace

GComponent* SkillMainPanel::container() const
{
    return getChild<GLoader>("container")->getComponent();
}

void SkillMainPanel::onCreate()
{
    addClickListener(getChild<GButton>("btnBack"), AX_CALLBACK_1(SkillMainPanel::onClickBack, this));

    auto* root     = container();
    m_itemList     = root->getChild("itemList")->as<GList>();
    m_titleText    = root->getChild("titleText")->as<GTextField>();
    m_detailText   = root->getChild("detailText")->as<GTextField>();
    m_btnConfig    = root->getChild("btnConfigSlots")->as<GButton>();

    m_titleText->setText("技能");
    m_btnConfig->setTitle("技能槽配置");
    addClickListener(m_btnConfig, AX_CALLBACK_1(SkillMainPanel::onClickConfigSlots, this));
    m_itemList->setDefaultItem(kItemUrl);
}

void SkillMainPanel::onShow()
{
    SkillLocalState::instance().ensureUnlockedForOccupation(occupation());
    rebuildList();
    refreshDetail();
}

void SkillMainPanel::onDestroy()
{
    m_itemList   = nullptr;
    m_titleText  = nullptr;
    m_detailText = nullptr;
    m_btnConfig  = nullptr;
}

void SkillMainPanel::onClickBack(EventContext*)
{
    close();
}

void SkillMainPanel::onClickConfigSlots(EventContext*)
{
    getUIManager()->open<SkillConfigPanel>();
}

int32_t SkillMainPanel::occupation() const
{
    auto* session = AppContext::get().gameSession();
    if (!session)
        return static_cast<int32_t>(mugen::CharacterClass::kSwordman);

    int32_t classId = session->selectedCharacter.classID;
    if (classId >= 101 && classId <= 104)
        classId = static_cast<int32_t>(mugen::type_conversions::toCharacterClass(classId));
    if (classId < static_cast<int32_t>(mugen::CharacterClass::kSwordman) ||
        classId > static_cast<int32_t>(mugen::CharacterClass::kMage))
    {
        AXLOGW("SkillMainPanel: invalid classID={}, defaulting to swordman", session->selectedCharacter.classID);
        return static_cast<int32_t>(mugen::CharacterClass::kSwordman);
    }
    return classId;
}

std::string SkillMainPanel::iconUrlForLeaf(int32_t leafId) const
{
    const auto* leaf = mugen::conf::Config::instance().find<mugen::conf::SkillLeafConfig>(leafId);
    if (!leaf)
        return {};
    return leaf->icon;
}

void SkillMainPanel::bindItem(GComponent* item, int32_t leafId, bool selected)
{
    auto* icon       = item->getChild("icon")->as<GLoader>();
    const auto* leaf = mugen::conf::Config::instance().find<mugen::conf::SkillLeafConfig>(leafId);
    icon->setURL(iconUrlForLeaf(leafId));

    if (auto* title = item->getChild("title"))
        title->setText(fmt::format("L{}", leafId));

    setItemSelectedVisual(item, selected);
    item->setData(reinterpret_cast<void*>(static_cast<intptr_t>(leafId)));
    (void)leaf;
}

void SkillMainPanel::rebuildList()
{
    const auto list = SkillLocalState::instance().listLeaves();
    m_listIds.clear();
    m_listIds.reserve(list.size());
    for (const auto& st : list)
        m_listIds.push_back(st.leafId);

    if (m_selectedLeafId == 0 && !m_listIds.empty())
        m_selectedLeafId = m_listIds.front();

    m_itemList->removeChildrenToPool();
    for (int32_t leafId : m_listIds)
    {
        auto* item = m_itemList->addItemFromPool()->as<GComponent>();
        bindItem(item, leafId, leafId == m_selectedLeafId);
        item->addClickListener([this, leafId](EventContext*) { onClickItem(leafId); });
    }
    refreshSelectionHighlights();
}

void SkillMainPanel::refreshSelectionHighlights()
{
    const int n = m_itemList->numChildren();
    for (int i = 0; i < n; ++i)
    {
        auto* item   = m_itemList->getChildAt(i)->as<GComponent>();
        const auto id = static_cast<int32_t>(reinterpret_cast<intptr_t>(item->getData()));
        setItemSelectedVisual(item, id == m_selectedLeafId);
    }
}

void SkillMainPanel::refreshDetail()
{
    if (m_selectedLeafId <= 0)
    {
        m_detailText->setText("Select a skill");
        return;
    }
    const auto* leaf = mugen::conf::Config::instance().find<mugen::conf::SkillLeafConfig>(m_selectedLeafId);
    if (!leaf)
    {
        m_detailText->setText(fmt::format("leaf {} missing", m_selectedLeafId));
        return;
    }

    std::string slotHint = "(not slotted)";
    for (const auto& b : SkillLocalState::instance().slotBindings())
    {
        if (b.leafId == m_selectedLeafId)
        {
            slotHint = fmt::format("slotted: {}", b.label);
            break;
        }
    }

    m_detailText->setText(fmt::format(
        "Leaf: {}\nSkillId: {}\nNameId: {}\nType: {}\nUnlockRoleLv: {}\n{}\n\n"
        "All profession skills unlocked (dev).\nOpen 技能槽配置 to assign loadout slots.",
        leaf->id, leaf->skillId, leaf->nameId, leaf->skillType, leaf->unlockRoleLevel, slotHint));
}

void SkillMainPanel::onClickItem(int32_t leafId)
{
    m_selectedLeafId = leafId;
    refreshSelectionHighlights();
    refreshDetail();
}

}  // namespace gameui
