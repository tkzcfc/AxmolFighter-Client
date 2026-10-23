#include "SkillConfigPanel.h"

#include "SkillLocalState.h"

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
const char* kSkillItemUrl = "ui://Panel_Skill/SkillItem";
const char* kSlotItemUrl  = "ui://Panel_Skill/SkillSlotItem";

void setItemSelectedVisual(GComponent* item, bool selected)
{
    if (auto* btn = dynamic_cast<GButton*>(item))
        btn->setSelected(selected);
    if (auto* glow = item->getChild("loaderSelectGlow"))
        glow->setVisible(selected);
}
}  // namespace

GComponent* SkillConfigPanel::container() const
{
    return getChild<GLoader>("container")->getComponent();
}

void SkillConfigPanel::onCreate()
{
    addClickListener(getChild<GButton>("btnBack"), AX_CALLBACK_1(SkillConfigPanel::onClickBack, this));

    auto* root    = container();
    m_slotList    = root->getChild("slotList")->as<GList>();
    m_skillList   = root->getChild("skillList")->as<GList>();
    m_titleText   = root->getChild("titleText")->as<GTextField>();
    m_hintText    = root->getChild("hintText")->as<GTextField>();
    m_btnClear    = root->getChild("btnClearSlot")->as<GButton>();
    m_btnReset    = root->getChild("btnReset")->as<GButton>();

    m_titleText->setText("技能槽配置");
    m_hintText->setText("先点技能槽，再点下方技能进行装配；点清空可移除。");
    m_btnClear->setTitle("清空槽位");
    m_btnReset->setTitle("重置默认");

    addClickListener(m_btnClear, AX_CALLBACK_1(SkillConfigPanel::onClickClearSlot, this));
    addClickListener(m_btnReset, AX_CALLBACK_1(SkillConfigPanel::onClickReset, this));

    m_slotList->setDefaultItem(kSlotItemUrl);
    m_skillList->setDefaultItem(kSkillItemUrl);
}

void SkillConfigPanel::onShow()
{
    SkillLocalState::instance().ensureUnlockedForOccupation(occupation());
    const auto bindings = SkillLocalState::instance().slotBindings();
    if (!bindings.empty())
        m_selectedSlot = bindings.front().slot;
    rebuildSlots();
    rebuildSkills();
}

void SkillConfigPanel::onDestroy()
{
    m_slotList  = nullptr;
    m_skillList = nullptr;
    m_titleText = nullptr;
    m_hintText  = nullptr;
    m_btnClear  = nullptr;
    m_btnReset  = nullptr;
}

void SkillConfigPanel::onClickBack(EventContext*)
{
    close();
}

void SkillConfigPanel::onClickClearSlot(EventContext*)
{
    SkillLocalState::instance().clearSlot(m_selectedSlot);
    rebuildSlots();
    rebuildSkills();
}

void SkillConfigPanel::onClickReset(EventContext*)
{
    SkillLocalState::instance().resetSlotsToDefault();
    rebuildSlots();
    rebuildSkills();
}

int32_t SkillConfigPanel::occupation() const
{
    auto* session = AppContext::get().gameSession();
    if (!session)
        return static_cast<int32_t>(mugen::CharacterClass::kSwordman);

    int32_t classId = session->selectedCharacter.classID;
    if (classId >= 101 && classId <= 104)
        classId = static_cast<int32_t>(mugen::type_conversions::toCharacterClass(classId));
    if (classId < static_cast<int32_t>(mugen::CharacterClass::kSwordman) ||
        classId > static_cast<int32_t>(mugen::CharacterClass::kMage))
        return static_cast<int32_t>(mugen::CharacterClass::kSwordman);
    return classId;
}

std::string SkillConfigPanel::iconUrlForLeaf(int32_t leafId) const
{
    const auto* leaf = mugen::conf::Config::instance().find<mugen::conf::SkillLeafConfig>(leafId);
    if (!leaf)
        return {};
    if (leaf->icon.rfind("ui://", 0) == 0)
        return leaf->icon;
    return {};
}

void SkillConfigPanel::bindSlotItem(GComponent* item, mugen::EnumSkillSlot slot, const char* label, int32_t leafId,
                                    bool selected)
{
    auto* icon = item->getChild("icon")->as<GLoader>();
    icon->setURL(leafId > 0 ? iconUrlForLeaf(leafId) : std::string());

    if (auto* title = item->getChild("title"))
    {
        if (leafId > 0)
            title->setText(fmt::format("{}\nL{}", label, leafId));
        else
            title->setText(fmt::format("{}\n—", label));
    }
    setItemSelectedVisual(item, selected);
    item->setData(reinterpret_cast<void*>(static_cast<intptr_t>(slot)));
}

void SkillConfigPanel::bindSkillItem(GComponent* item, int32_t leafId, bool selected)
{
    auto* icon = item->getChild("icon")->as<GLoader>();
    icon->setURL(iconUrlForLeaf(leafId));
    if (auto* title = item->getChild("title"))
        title->setText(fmt::format("L{}", leafId));
    setItemSelectedVisual(item, selected);
    item->setData(reinterpret_cast<void*>(static_cast<intptr_t>(leafId)));
}

void SkillConfigPanel::rebuildSlots()
{
    const auto bindings = SkillLocalState::instance().slotBindings();
    m_slotList->removeChildrenToPool();
    for (const auto& b : bindings)
    {
        auto* item = m_slotList->addItemFromPool()->as<GComponent>();
        bindSlotItem(item, b.slot, b.label, b.leafId, b.slot == m_selectedSlot);
        const auto slot = b.slot;
        item->addClickListener([this, slot](EventContext*) { onClickSlot(slot); });
    }
}

void SkillConfigPanel::rebuildSkills()
{
    const auto list = SkillLocalState::instance().listLeaves();
    m_skillList->removeChildrenToPool();
    for (const auto& st : list)
    {
        auto* item = m_skillList->addItemFromPool()->as<GComponent>();
        bindSkillItem(item, st.leafId, st.leafId == m_selectedLeafId);
        const int32_t leafId = st.leafId;
        item->addClickListener([this, leafId](EventContext*) { onClickSkill(leafId); });
    }
}

void SkillConfigPanel::refreshSlotHighlights()
{
    const int n = m_slotList->numChildren();
    for (int i = 0; i < n; ++i)
    {
        auto* item  = m_slotList->getChildAt(i)->as<GComponent>();
        const auto s = static_cast<mugen::EnumSkillSlot>(reinterpret_cast<intptr_t>(item->getData()));
        setItemSelectedVisual(item, s == m_selectedSlot);
    }
}

void SkillConfigPanel::refreshSkillHighlights()
{
    const int n = m_skillList->numChildren();
    for (int i = 0; i < n; ++i)
    {
        auto* item  = m_skillList->getChildAt(i)->as<GComponent>();
        const auto id = static_cast<int32_t>(reinterpret_cast<intptr_t>(item->getData()));
        setItemSelectedVisual(item, id == m_selectedLeafId);
    }
}

void SkillConfigPanel::onClickSlot(mugen::EnumSkillSlot slot)
{
    m_selectedSlot = slot;
    m_selectedLeafId = SkillLocalState::instance().leafInSlot(slot);
    refreshSlotHighlights();
    refreshSkillHighlights();
    AXLOGI("SkillConfigPanel: selected slot {}", static_cast<int>(slot));
}

void SkillConfigPanel::onClickSkill(int32_t leafId)
{
    m_selectedLeafId = leafId;
    SkillLocalState::instance().assignSlot(m_selectedSlot, leafId);
    rebuildSlots();
    refreshSkillHighlights();
}

}  // namespace gameui
