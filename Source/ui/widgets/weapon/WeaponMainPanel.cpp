#include "WeaponMainPanel.h"

#include "WeaponLocalState.h"

#include "AppContext.h"
#include "mugen/conf/ReflectedConfig.h"
#include "fairygui/UIPackage.h"

#include <algorithm>
#include <fmt/format.h>

namespace gameui
{

namespace
{
const char* kItemUrl = "ui://Panel_Weapon/WeaponItem";

void setItemSelectedVisual(GComponent* item, bool selected)
{
    if (auto* btn = dynamic_cast<GButton*>(item))
        btn->setSelected(selected);
    if (auto* glow = item->getChild("loaderSelectGlow"))
        glow->setVisible(selected);
}
}  // namespace

GComponent* WeaponMainPanel::container() const
{
    return getChild<GLoader>("container")->getComponent();
}

void WeaponMainPanel::onCreate()
{
    addClickListener(getChild<GButton>("btnBack"), AX_CALLBACK_1(WeaponMainPanel::onClickBack, this));

    auto* root     = container();
    m_itemList     = root->getChild("itemList")->as<GList>();
    m_titleText    = root->getChild("titleText")->as<GTextField>();
    m_detailText   = root->getChild("detailText")->as<GTextField>();
    m_previewIcon  = root->getChild("previewIcon")->as<GLoader>();
    m_btnAppearance = root->getChild("btnAppearance")->as<GButton>();
    m_btnSkill      = root->getChild("btnSkill")->as<GButton>();
    m_btnLevel      = root->getChild("btnLevel")->as<GButton>();
    m_btnStar       = root->getChild("btnStar")->as<GButton>();
    m_btnGrade      = root->getChild("btnGrade")->as<GButton>();

    m_btnAppearance->setTitle("设为外观");
    m_btnSkill->setTitle("技能+");
    m_btnLevel->setTitle("升级");
    m_btnStar->setTitle("升星");
    m_btnGrade->setTitle("进阶");

    addClickListener(m_btnAppearance, AX_CALLBACK_1(WeaponMainPanel::onClickAppearance, this));
    addClickListener(m_btnSkill, AX_CALLBACK_1(WeaponMainPanel::onClickSkill, this));
    addClickListener(m_btnLevel, AX_CALLBACK_1(WeaponMainPanel::onClickLevelUp, this));
    addClickListener(m_btnStar, AX_CALLBACK_1(WeaponMainPanel::onClickStarUp, this));
    addClickListener(m_btnGrade, AX_CALLBACK_1(WeaponMainPanel::onClickGradeUp, this));

    m_itemList->setDefaultItem(kItemUrl);
}

void WeaponMainPanel::onShow()
{
    WeaponLocalState::instance().ensureUnlockedForOccupation(occupation());
    rebuildList();
    refreshDetail();
}

void WeaponMainPanel::onDestroy()
{
    m_itemList      = nullptr;
    m_titleText     = nullptr;
    m_detailText    = nullptr;
    m_previewIcon   = nullptr;
    m_btnAppearance = nullptr;
    m_btnSkill      = nullptr;
    m_btnLevel      = nullptr;
    m_btnStar       = nullptr;
    m_btnGrade      = nullptr;
}

void WeaponMainPanel::onClickBack(EventContext*)
{
    close();
}

int32_t WeaponMainPanel::occupation() const
{
    auto* session = AppContext::get().gameSession();
    return session ? session->selectedCharacter.classID : 1;
}

const mugen::conf::ArmamentConfig* WeaponMainPanel::findConfig(int32_t armamentId) const
{
    const auto* cfg =
        mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>({armamentId, occupation()});
    if (!cfg)
        cfg = mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>({armamentId, 0});
    return cfg;
}

void WeaponMainPanel::bindItem(GComponent* item, int32_t armamentId, bool selected)
{
    auto* icon = item->getChild("icon")->as<GLoader>();
    const auto* cfg = findConfig(armamentId);
    const auto* inst = WeaponLocalState::instance().find(armamentId);
    // Prefer list icon URL from config (already ui://Res_WeaponIcons/...).
    std::string url;
    if (cfg)
        url = !cfg->iconList.empty() ? cfg->iconList : cfg->icon;
    icon->setURL(url);

    const bool isAppearance = (WeaponLocalState::instance().appearanceId() == armamentId);
    if (auto* title = item->getChild("title"))
    {
        title->setText(fmt::format("#{} L{}★{}{}", armamentId, inst ? inst->level : 1, inst ? inst->star : 0,
                                   isAppearance ? "外观" : ""));
    }
    setItemSelectedVisual(item, selected);
    item->setData(reinterpret_cast<void*>(static_cast<intptr_t>(armamentId)));
}

void WeaponMainPanel::rebuildList()
{
    auto& state     = WeaponLocalState::instance();
    const auto list = state.listForOccupation(occupation());
    m_listIds.clear();
    m_itemList->removeChildrenToPool();
    m_itemList->getItemPool()->clear();
    m_itemList->setNumItems(0);

    for (const auto& inst : list)
    {
        m_listIds.push_back(inst.configId);
        auto* item = m_itemList->addItemFromPool(kItemUrl)->as<GButton>();
        const bool selected = (m_selectedId == inst.configId);
        bindItem(item, inst.configId, selected);
        const int32_t id = inst.configId;
        addClickListener(item, [this, id](EventContext*) { onClickItem(id); });
    }

    if (m_selectedId == 0 && !m_listIds.empty())
        m_selectedId = m_listIds.front();

    if (m_titleText)
        m_titleText->setText(fmt::format("武装 / Armament ({} items)", m_listIds.size()));

    AXLOGI("WeaponMainPanel: listed {} armaments (occupation={})", m_listIds.size(), occupation());
}

void WeaponMainPanel::refreshSelectionHighlights()
{
    for (int i = 0; i < m_itemList->numChildren(); ++i)
    {
        auto* item = m_itemList->getChildAt(i)->as<GComponent>();
        const auto id = static_cast<int32_t>(reinterpret_cast<intptr_t>(item->getData()));
        bindItem(item, id, id == m_selectedId);
    }
}

void WeaponMainPanel::onClickItem(int32_t armamentId)
{
    m_selectedId = armamentId;
    refreshSelectionHighlights();
    refreshDetail();
}

void WeaponMainPanel::refreshDetail()
{
    auto& state      = WeaponLocalState::instance();
    const auto* inst = state.find(m_selectedId);
    const auto* cfg  = findConfig(m_selectedId);

    if (!inst || !cfg)
    {
        m_detailText->setText("No selection");
        if (m_previewIcon)
            m_previewIcon->setURL("");
        return;
    }

    const std::string& preview = !cfg->iconBig.empty() ? cfg->iconBig : (!cfg->iconList.empty() ? cfg->iconList : cfg->icon);
    if (m_previewIcon)
        m_previewIcon->setURL(preview);

    m_detailText->setText(fmt::format(
        "ID: {}\nnameId: {}\nquality: {}\noccupation: {}\n"
        "level: {} / {}\nstar: {} / {}\ngrade: {} / {}\n"
        "skillId: {} (comboSkill={})\nskinId: {}\n"
        "appearance: {}\nicon: {}",
        cfg->id, cfg->nameId, cfg->quality, cfg->occupation, inst->level, WeaponLocalState::kMaxLevel, inst->star,
        WeaponLocalState::kMaxStar, inst->grade, WeaponLocalState::kMaxGrade, inst->skillId, cfg->comboSkill,
        inst->skinId, state.appearanceId() == inst->configId ? "YES" : "no", preview));
}

void WeaponMainPanel::onClickAppearance(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    WeaponLocalState::instance().setAppearance(m_selectedId);
    if (const auto* cfg = findConfig(m_selectedId))
    {
        if (cfg->skin != 0)
            WeaponLocalState::instance().setSkin(m_selectedId, cfg->skin);
    }
    refreshSelectionHighlights();
    refreshDetail();
}

void WeaponMainPanel::onClickSkill(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    auto* inst = WeaponLocalState::instance().find(m_selectedId);
    if (!inst)
        return;
    const auto* cfg  = findConfig(m_selectedId);
    const int32_t base = cfg ? cfg->comboSkill : inst->skillId;
    if (inst->skillId == base)
        WeaponLocalState::instance().setSkill(m_selectedId, base + 1);
    else if (inst->skillId == base + 1)
        WeaponLocalState::instance().setSkill(m_selectedId, base + 2);
    else
        WeaponLocalState::instance().setSkill(m_selectedId, base);
    refreshDetail();
}

void WeaponMainPanel::onClickLevelUp(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    WeaponLocalState::instance().addLevel(m_selectedId);
    refreshSelectionHighlights();
    refreshDetail();
}

void WeaponMainPanel::onClickStarUp(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    WeaponLocalState::instance().addStar(m_selectedId);
    refreshSelectionHighlights();
    refreshDetail();
}

void WeaponMainPanel::onClickGradeUp(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    WeaponLocalState::instance().addGrade(m_selectedId);
    refreshSelectionHighlights();
    refreshDetail();
}

}  // namespace gameui
