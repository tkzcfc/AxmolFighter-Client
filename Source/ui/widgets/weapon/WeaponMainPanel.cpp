#include "WeaponMainPanel.h"

#include "WeaponLocalState.h"

#include "AppContext.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/tables/ArmamentTables.h"
#include "fairygui/GGraph.h"
#include "fairygui/GList.h"
#include "fairygui/GTextField.h"
#include "fairygui/UIPackage.h"

#include <fmt/format.h>

#include <algorithm>

namespace gameui
{

namespace
{
GButton* makeCommonButton(const char* resName, const char* title)
{
    auto* btn = UIPackage::createObject("Common_Base", resName)->as<GButton>();
    if (title && *title)
        btn->setTitle(title);
    return btn;
}

GBasicTextField* makeLabel(const std::string& text, int fontSize, ax::Color3B color)
{
    auto* tf = GBasicTextField::create();
    tf->setFontSize(static_cast<float>(fontSize));
    tf->setColor(color);
    tf->setText(text);
    tf->setAutoSize(AutoSizeType::BOTH);
    return tf;
}
}  // namespace

GComponent* WeaponMainPanel::onCreatePanelContent()
{
    auto* root = GComponent::create();
    root->setSize(1280, 720);
    root->name = "WeaponMainPanel";

    auto* bg = GGraph::create();
    bg->setSize(1280, 720);
    bg->drawRect(1280, 720, 0, ax::Color4F(0, 0, 0, 0), ax::Color4F(0.08f, 0.09f, 0.12f, 0.94f));
    bg->addRelation(root, RelationType::Size);
    root->addChild(bg);

    auto* back = makeCommonButton("ButtonBack", "");
    back->name = "btnBack";
    back->setPosition(0, 0);
    root->addChild(back);

    m_titleText = makeLabel("武装 / Armament (dev)", 28, ax::Color3B::WHITE);
    m_titleText->name = "titleText";
    m_titleText->setPosition(80, 24);
    root->addChild(m_titleText);

    m_itemList = GList::create();
    m_itemList->name = "itemList";
    m_itemList->setPosition(40, 80);
    m_itemList->setSize(520, 600);
    m_itemList->setLayout(ListLayoutType::SINGLE_COLUMN);
    m_itemList->setLineGap(6);
    // setupOverflow is protected; page the list instead of ScrollPane for this vertical slice.
    m_itemList->setDefaultItem("ui://Common_Base/ButtonGeneral");
    root->addChild(m_itemList);

    m_detailText = makeLabel("Select an armament", 20, ax::Color3B(220, 220, 220));
    m_detailText->name = "detailText";
    m_detailText->setPosition(600, 100);
    m_detailText->setSize(620, 280);
    m_detailText->setAutoSize(AutoSizeType::HEIGHT);
    m_detailText->setUBBEnabled(true);
    root->addChild(m_detailText);

    auto placeBtn = [&](GButton*& slot, const char* name, const char* title, float x, float y) {
        slot = makeCommonButton("ButtonYes", title);
        slot->name = name;
        slot->setPosition(x, y);
        root->addChild(slot);
    };
    placeBtn(m_btnAppearance, "btnAppearance", "设为外观", 600, 420);
    placeBtn(m_btnSkill, "btnSkill", "技能+ (cycle)", 780, 420);
    placeBtn(m_btnLevel, "btnLevel", "升级 (无消耗)", 600, 500);
    placeBtn(m_btnStar, "btnStar", "升星 (无消耗)", 780, 500);
    placeBtn(m_btnGrade, "btnGrade", "进阶 (无消耗)", 960, 500);
    placeBtn(m_btnPrevPage, "btnPrevPage", "上一页", 40, 690);
    placeBtn(m_btnNextPage, "btnNextPage", "下一页", 200, 690);

    auto* hint = makeLabel("Cheat: all unlocked. Local only. Use pages to browse list.", 16,
                           ax::Color3B(160, 160, 160));
    hint->setPosition(600, 580);
    root->addChild(hint);

    return root;
}

void WeaponMainPanel::onCreate()
{
    addClickListener(getChild<GButton>("btnBack"), AX_CALLBACK_1(WeaponMainPanel::onClickBack, this));
    m_itemList      = getChild<GList>("itemList");
    m_titleText     = getChild<GTextField>("titleText");
    m_detailText    = getChild<GTextField>("detailText");
    m_btnAppearance = getChild<GButton>("btnAppearance");
    m_btnSkill      = getChild<GButton>("btnSkill");
    m_btnLevel      = getChild<GButton>("btnLevel");
    m_btnStar       = getChild<GButton>("btnStar");
    m_btnGrade      = getChild<GButton>("btnGrade");

    addClickListener(m_btnAppearance, AX_CALLBACK_1(WeaponMainPanel::onClickAppearance, this));
    addClickListener(m_btnSkill, AX_CALLBACK_1(WeaponMainPanel::onClickSkill, this));
    addClickListener(m_btnLevel, AX_CALLBACK_1(WeaponMainPanel::onClickLevelUp, this));
    addClickListener(m_btnStar, AX_CALLBACK_1(WeaponMainPanel::onClickStarUp, this));
    addClickListener(m_btnGrade, AX_CALLBACK_1(WeaponMainPanel::onClickGradeUp, this));
    m_btnPrevPage = getChild<GButton>("btnPrevPage");
    m_btnNextPage = getChild<GButton>("btnNextPage");
    addClickListener(m_btnPrevPage, AX_CALLBACK_1(WeaponMainPanel::onClickPrevPage, this));
    addClickListener(m_btnNextPage, AX_CALLBACK_1(WeaponMainPanel::onClickNextPage, this));
}

void WeaponMainPanel::onShow()
{
    auto& state = WeaponLocalState::instance();
    state.ensureUnlockedForOccupation(occupation());
    rebuildList();
    refreshDetail();
}

void WeaponMainPanel::onDestroy()
{
    m_itemList      = nullptr;
    m_titleText     = nullptr;
    m_detailText    = nullptr;
    m_btnAppearance = nullptr;
    m_btnSkill      = nullptr;
    m_btnLevel      = nullptr;
    m_btnStar       = nullptr;
    m_btnGrade      = nullptr;
    m_btnPrevPage   = nullptr;
    m_btnNextPage   = nullptr;
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

void WeaponMainPanel::rebuildList()
{
    auto& state     = WeaponLocalState::instance();
    const auto list = state.listForOccupation(occupation());
    m_listIds.clear();
    for (const auto& inst : list)
        m_listIds.push_back(inst.configId);

    const int total = static_cast<int>(m_listIds.size());
    const int pages = std::max(1, (total + kPageSize - 1) / kPageSize);
    m_page          = std::clamp(m_page, 0, pages - 1);
    const int begin = m_page * kPageSize;
    const int end   = std::min(begin + kPageSize, total);

    m_itemList->removeChildrenToPool();
    m_itemList->setNumItems(0);

    for (int i = begin; i < end; ++i)
    {
        const int32_t id = m_listIds[static_cast<size_t>(i)];
        const auto* inst = state.find(id);
        if (!inst)
            continue;
        auto* item              = m_itemList->addItemFromPool()->as<GButton>();
        const bool isAppearance = (state.appearanceId() == id);
        const bool selected     = (m_selectedId == id);
        item->setTitle(fmt::format("{}#{}  Lv{} ★{} G{}{}", selected ? "> " : "", id, inst->level, inst->star,
                                   inst->grade, isAppearance ? " [外观]" : ""));
        item->setSelected(selected);
        addClickListener(item, [this, id](EventContext*) { onClickItem(id); });
    }

    if (m_selectedId == 0 && !m_listIds.empty())
        m_selectedId = m_listIds.front();

    if (m_titleText)
        m_titleText->setText(fmt::format("武装 / Armament (page {}/{}, {} items)", m_page + 1, pages, total));

    AXLOGI("WeaponMainPanel: listed page {}/{} ({} items, occupation={})", m_page + 1, pages, total, occupation());
}

void WeaponMainPanel::onClickItem(int32_t armamentId)
{
    m_selectedId = armamentId;
    rebuildList();
    refreshDetail();
}

void WeaponMainPanel::refreshDetail()
{
    auto& state      = WeaponLocalState::instance();
    const auto* inst = state.find(m_selectedId);
    const auto* cfg =
        mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>({m_selectedId, occupation()});
    if (!cfg)
        cfg = mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>({m_selectedId, 0});

    if (!inst || !cfg)
    {
        m_detailText->setText("No selection");
        return;
    }

    m_detailText->setText(fmt::format(
        "ID: {}\nnameId: {}\nquality: {}\noccupation: {}\n"
        "level: {} / {}\nstar: {} / {}\ngrade: {} / {}\n"
        "skillId: {} (table comboSkill={})\nskinId: {}\n"
        "appearance: {}\n"
        "icon: {}",
        cfg->id, cfg->nameId, cfg->quality, cfg->occupation, inst->level, WeaponLocalState::kMaxLevel, inst->star,
        WeaponLocalState::kMaxStar, inst->grade, WeaponLocalState::kMaxGrade, inst->skillId, cfg->comboSkill,
        inst->skinId, state.appearanceId() == inst->configId ? "YES" : "no",
        cfg->iconList.empty() ? cfg->icon : cfg->iconList));
}

void WeaponMainPanel::onClickAppearance(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    WeaponLocalState::instance().setAppearance(m_selectedId);
    if (const auto* cfg =
            mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>({m_selectedId, occupation()}))
    {
        if (cfg->skin != 0)
            WeaponLocalState::instance().setSkin(m_selectedId, cfg->skin);
    }
    rebuildList();
    refreshDetail();
}

void WeaponMainPanel::onClickSkill(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    auto* inst = WeaponLocalState::instance().find(m_selectedId);
    if (!inst)
        return;
    const auto* cfg =
        mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>({m_selectedId, occupation()});
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
    rebuildList();
    refreshDetail();
}

void WeaponMainPanel::onClickStarUp(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    WeaponLocalState::instance().addStar(m_selectedId);
    rebuildList();
    refreshDetail();
}

void WeaponMainPanel::onClickGradeUp(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    WeaponLocalState::instance().addGrade(m_selectedId);
    rebuildList();
    refreshDetail();
}

void WeaponMainPanel::onClickPrevPage(EventContext*)
{
    if (m_page > 0)
    {
        --m_page;
        rebuildList();
    }
}

void WeaponMainPanel::onClickNextPage(EventContext*)
{
    ++m_page;
    rebuildList();
}

}  // namespace gameui
