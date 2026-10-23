#include "WeaponMainPanel.h"

#include "WeaponLocalState.h"
#include "WeaponSkillPreviewDialog.h"
#include "WeaponFormPreviewDialog.h"

#include "AppContext.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/GameDef.h"
#include "mugen/render/spine/MgSkeletonAnimation.h"
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

    auto* root          = container();
    m_itemList          = root->getChild("itemList")->as<GList>();
    m_titleText         = root->getChild("titleText")->as<GTextField>();
    m_detailText        = root->getChild("detailText")->as<GTextField>();
    m_previewSpine      = root->getChild("previewSpine")->as<GLoader3D>();
    m_btnAppearance     = root->getChild("btnAppearance")->as<GButton>();
    m_btnSkill          = root->getChild("btnSkill")->as<GButton>();
    m_btnLevel          = root->getChild("btnLevel")->as<GButton>();
    m_btnStar           = root->getChild("btnStar")->as<GButton>();
    m_btnGrade          = root->getChild("btnGrade")->as<GButton>();
    m_btnSkillPreview   = root->getChild("btnSkillPreview")->as<GButton>();
    m_btnFormPreview    = root->getChild("btnFormPreview")->as<GButton>();

    m_btnAppearance->setTitle("设为外观");
    m_btnSkill->setTitle("技能+");
    m_btnLevel->setTitle("升级");
    m_btnStar->setTitle("升星");
    m_btnGrade->setTitle("进阶");
    m_btnSkillPreview->setTitle("技能预览");
    m_btnFormPreview->setTitle("形态预览");

    addClickListener(m_btnAppearance, AX_CALLBACK_1(WeaponMainPanel::onClickAppearance, this));
    addClickListener(m_btnSkill, AX_CALLBACK_1(WeaponMainPanel::onClickSkill, this));
    addClickListener(m_btnLevel, AX_CALLBACK_1(WeaponMainPanel::onClickLevelUp, this));
    addClickListener(m_btnStar, AX_CALLBACK_1(WeaponMainPanel::onClickStarUp, this));
    addClickListener(m_btnGrade, AX_CALLBACK_1(WeaponMainPanel::onClickGradeUp, this));
    addClickListener(m_btnSkillPreview, AX_CALLBACK_1(WeaponMainPanel::onClickSkillPreview, this));
    addClickListener(m_btnFormPreview, AX_CALLBACK_1(WeaponMainPanel::onClickFormPreview, this));

    m_itemList->setDefaultItem(kItemUrl);
}

void WeaponMainPanel::onShow()
{
    WeaponLocalState::instance().ensureUnlockedForOccupation(occupation());
    rebuildList();
    refreshDetail();
    refreshSpinePreview();
}

void WeaponMainPanel::onDestroy()
{
    if (m_previewSpine)
        m_previewSpine->setContent(nullptr);
    m_itemList        = nullptr;
    m_titleText       = nullptr;
    m_detailText      = nullptr;
    m_previewSpine    = nullptr;
    m_btnAppearance   = nullptr;
    m_btnSkill        = nullptr;
    m_btnLevel        = nullptr;
    m_btnStar         = nullptr;
    m_btnGrade        = nullptr;
    m_btnSkillPreview = nullptr;
    m_btnFormPreview  = nullptr;
    m_previewSpineId  = 0;
}

void WeaponMainPanel::onClickBack(EventContext*)
{
    close();
}

int32_t WeaponMainPanel::occupation() const
{
    auto* session = AppContext::get().gameSession();
    if (!session)
        return static_cast<int32_t>(mugen::CharacterClass::kSwordman);

    // classID is CharacterClass (1..4). Tolerate accidental roleConfigId (101..104).
    int32_t classId = session->selectedCharacter.classID;
    if (classId >= 101 && classId <= 104)
        classId = static_cast<int32_t>(mugen::type_conversions::toCharacterClass(classId));
    if (classId < static_cast<int32_t>(mugen::CharacterClass::kSwordman) ||
        classId > static_cast<int32_t>(mugen::CharacterClass::kMage))
    {
        AXLOGW("WeaponMainPanel: invalid classID={}, defaulting to swordman", session->selectedCharacter.classID);
        return static_cast<int32_t>(mugen::CharacterClass::kSwordman);
    }
    return classId;
}

const mugen::conf::ArmamentConfig* WeaponMainPanel::findConfig(int32_t armamentId) const
{
    const int32_t occ = occupation();
    const auto* cfg =
        mugen::conf::Config::instance().find<mugen::conf::ArmamentConfig>({armamentId, occ});
    // Occupation-scoped only: never fall back to another job's row (wrong skill video).
    if (cfg && cfg->occupation == occ)
        return cfg;

    // Linear scan fallback if archive sort/lookup ever mismatches.
    for (const auto& row : mugen::conf::Config::instance().table<mugen::conf::ArmamentConfig>())
    {
        if (row.id == armamentId && row.occupation == occ)
            return &row;
    }
    AXLOGW("WeaponMainPanel: no armament row id={} occupation={}", armamentId, occ);
    return nullptr;
}

void WeaponMainPanel::bindItem(GComponent* item, int32_t armamentId, bool selected)
{
    auto* icon       = item->getChild("icon")->as<GLoader>();
    const auto* cfg  = findConfig(armamentId);
    const auto* inst = WeaponLocalState::instance().find(armamentId);
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
        auto* item          = m_itemList->addItemFromPool(kItemUrl)->as<GButton>();
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
    refreshSpinePreview();
}

void WeaponMainPanel::refreshDetail()
{
    auto& state      = WeaponLocalState::instance();
    const auto* inst = state.find(m_selectedId);
    const auto* cfg  = findConfig(m_selectedId);

    if (!inst || !cfg)
    {
        m_detailText->setText("No selection");
        return;
    }

    m_detailText->setText(fmt::format(
        "ID: {}  nameId: {}  quality: {}  occupation: {}\n"
        "level: {}/{}  star: {}/{}  grade: {}/{}\n"
        "skillId: {} (combo={})  appearance: {}\n"
        "spineId: {}  anim: {}  videoId: {}\n"
        "video: {}",
        cfg->id, cfg->nameId, cfg->quality, cfg->occupation, inst->level, WeaponLocalState::kMaxLevel, inst->star,
        WeaponLocalState::kMaxStar, inst->grade, WeaponLocalState::kMaxGrade, inst->skillId, cfg->comboSkill,
        state.appearanceId() == inst->configId ? "YES" : "no", cfg->spineId, cfg->spineAnim, cfg->video,
        cfg->videoPath.empty() ? "(none)" : cfg->videoPath));
}

void WeaponMainPanel::refreshSpinePreview()
{
    if (!m_previewSpine)
        return;

    const auto* cfg = findConfig(m_selectedId);
    if (!cfg || cfg->spineId <= 0)
    {
        m_previewSpine->setContent(nullptr);
        m_previewSpineId = 0;
        return;
    }

    if (m_previewSpineId == cfg->spineId && m_previewSpine->getContent() != nullptr)
        return;

    m_previewSpineId = cfg->spineId;
    m_previewSpine->setContent(nullptr);

    const int32_t spineId   = cfg->spineId;
    const std::string anim  = cfg->spineAnim.empty() ? "stand" : cfg->spineAnim;
    const int32_t selected  = m_selectedId;
    auto weakThis           = weak_from_this();

    mugen::MgSkeletonAnimation::createFromCacheAsync(
        spineId, [weakThis, this, spineId, anim, selected](mugen::MgSkeletonAnimation* skel) {
            if (!skel)
            {
                AXLOGW("WeaponMainPanel: spine load failed id={}", spineId);
                return;
            }
            if (weakThis.expired())
                return;
            if (m_selectedId != selected || m_previewSpineId != spineId || !m_previewSpine)
                return;

            skel->setPosition(ax::Vec2(m_previewSpine->getWidth() * 0.5f, 40.0f));
            skel->setScale(0.8f);
            skel->setAnimation(0, anim, true);
            m_previewSpine->setContent(skel);
            AXLOGI("WeaponMainPanel: preview spineId={} anim={}", spineId, anim);
        });
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
    const auto* cfg    = findConfig(m_selectedId);
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

void WeaponMainPanel::onClickSkillPreview(EventContext*)
{
    const auto* cfg = findConfig(m_selectedId);
    if (!cfg)
        return;
    // Guaranteed same occupation as the listed icon / form preview selection.
    AXLOGI("WeaponMainPanel: skill preview id={} occupation={} video={} path={}", cfg->id, cfg->occupation,
           cfg->video, cfg->videoPath.empty() ? "(none)" : cfg->videoPath);
    getUIManager()->open<WeaponSkillPreviewDialog>(cfg->videoPath);
}

void WeaponMainPanel::onClickFormPreview(EventContext*)
{
    const auto* cfg = findConfig(m_selectedId);
    if (!cfg || cfg->spineId <= 0)
    {
        AXLOGW("WeaponMainPanel: form preview missing spine for id={}", m_selectedId);
        return;
    }
    getUIManager()->open<WeaponFormPreviewDialog>(cfg->spineId);
}

}  // namespace gameui
