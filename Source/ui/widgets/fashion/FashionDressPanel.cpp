#include "FashionDressPanel.h"

#include "FashionMainPanel.h"

#include "ui/utils/UiQualityRes.h"

#include "AppContext.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "fairygui/GLoader3D.h"
#include "fairygui/UIPackage.h"

#include <algorithm>
#include <string>
#include "ui/progress/FeatureProgressSync.h"
#include "core/event/EventDispatcher.h"

namespace gameui
{

namespace
{
const char* partTitle(mugen::FashionPosition pos)
{
    using FP = mugen::FashionPosition;
    switch (pos)
    {
    case FP::kHat:
        return "头饰";
    case FP::kClothes:
        return "衣服";
    case FP::kHair:
        return "头发";
    case FP::kFace:
        return "脸饰";
    case FP::kWing:
        return "翅膀";
    case FP::kHalo:
        return "光环";
    case FP::kPhantom:
        return "幻影";
    default:
        return "部位";
    }
}

// Tabs match heiyue ConstUI.UI_FASHION_APPAREL (weapon tab omitted).
constexpr mugen::FashionPosition kPartTabs[] = {
    mugen::FashionPosition::kHat,     mugen::FashionPosition::kClothes, mugen::FashionPosition::kHair,
    mugen::FashionPosition::kFace,    mugen::FashionPosition::kWing,    mugen::FashionPosition::kHalo,
    mugen::FashionPosition::kPhantom,
};

const char* kSuitItemUrl = "ui://Panel_Fashion/FashionSuitItem";
const char* kPartItemUrl = "ui://Panel_Fashion/FashionItem";

void setItemSelectedVisual(GComponent* item, bool selected)
{
    if (auto* btn = dynamic_cast<GButton*>(item))
        btn->setSelected(selected);
    if (auto* glow = item->getChild("loaderSelectGlow"))
        glow->setVisible(selected);
}
}  // namespace

FashionDressPanel::FashionDressPanel(std::optional<mugen::FashionPosition> initialPart)
    : m_initialPart(std::move(initialPart)), m_activePart(m_initialPart)
{
}

GComponent* FashionDressPanel::container() const
{
    return getChild<GLoader>("container")->getComponent();
}

void FashionDressPanel::onCreate()
{
    addClickListener(getChild<GButton>("btnBack"), AX_CALLBACK_1(FashionDressPanel::onClickBack, this));

    auto* root     = container();
    m_tabList      = root->getChild("tabList")->as<GList>();
    m_itemList     = root->getChild("itemList")->as<GList>();
    m_avatarLoader = root->getChild("loaderAvatar")->as<GLoader3D>();
    m_titleText    = root->getChild("titleText")->as<GTextField>();
    m_saveButton   = root->getChild("btnSave")->as<GButton>();
    m_saveButton->setTitle("保存");
    addClickListener(m_saveButton, AX_CALLBACK_1(FashionDressPanel::onClickSave, this));

    if (auto* walk = root->getChild("btnToWalk"))
    {
        m_walkButton = walk->as<GButton>();
        addClickListener(m_walkButton, AX_CALLBACK_1(FashionDressPanel::onClickWalk, this));
    }

    loadDraftFromSession();
    rebuildTabs();
}

void FashionDressPanel::onShow()
{
    rebuildItemList();
    refreshAvatar();
}

void FashionDressPanel::onDestroy()
{
    if (m_avatarLoader)
        m_avatarLoader->setContent(nullptr);
    m_avatarLoader = nullptr;
    m_avatar       = nullptr;
    FashionMainPanel::requestRefresh();
}

void FashionDressPanel::onClickBack(EventContext*)
{
    close();
}

void FashionDressPanel::onClickWalk(EventContext*)
{
    m_walkPose = !m_walkPose;
    if (m_walkButton)
        m_walkButton->setSelected(m_walkPose);
    refreshAvatar();
}

void FashionDressPanel::onClickSave(EventContext*)
{
    saveDraftToSession();
    AXLOGI("FashionDressPanel: save applied locally (network wear protocol not implemented)");
    close();
}

void FashionDressPanel::loadDraftFromSession()
{
    m_draftEquip.clear();
    auto* session = AppContext::get().gameSession();
    if (!session)
        return;
    for (const auto& f : session->selectedCharacter.fashions)
    {
        if (f.worn && f.configID > 0)
            m_draftEquip[static_cast<mugen::FashionPosition>(f.position)] = f.configID;
    }
}

int32_t FashionDressPanel::occupation() const
{
    auto* session = AppContext::get().gameSession();
    return session ? session->selectedCharacter.classID : 0;
}

int32_t FashionDressPanel::roleId() const
{
    return mugen::type_conversions::toRoleConfigId(static_cast<mugen::CharacterClass>(occupation()));
}

const mugen::conf::FashionConfig* FashionDressPanel::findFashion(int32_t fashionId) const
{
    auto& cfg       = mugen::conf::Config::instance();
    const auto* row = cfg.find<mugen::conf::FashionConfig>({fashionId, occupation()});
    if (!row)
        row = cfg.find<mugen::conf::FashionConfig>({fashionId, 0});
    return row;
}

void FashionDressPanel::rebuildTabs()
{
    m_tabs.clear();
    m_tabs.push_back({std::nullopt, "套装"});
    for (auto part : kPartTabs)
        m_tabs.push_back({part, partTitle(part)});

    int selected = 0;
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
    {
        if (m_tabs[static_cast<size_t>(i)].part == m_activePart)
        {
            selected = i;
            break;
        }
    }

    m_tabList->removeChildrenToPool();
    m_tabList->setNumItems(0);
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
    {
        auto* item = m_tabList->addItemFromPool()->as<GButton>();
        item->setTitle(m_tabs[static_cast<size_t>(i)].title);
        item->setSelected(i == selected);
        const int index = i;
        addClickListener(item, [this, index](EventContext*) { onClickTab(index); });
    }
    onClickTab(selected);
}

void FashionDressPanel::onClickTab(int index)
{
    if (index < 0 || index >= static_cast<int>(m_tabs.size()))
        return;
    m_activePart        = m_tabs[static_cast<size_t>(index)].part;
    m_selectedSuitId    = 0;
    m_selectedFashionId = 0;
    m_titleText->setText(m_tabs[static_cast<size_t>(index)].title);
    for (int i = 0; i < m_tabList->numChildren(); ++i)
        m_tabList->getChildAt(i)->as<GButton>()->setSelected(i == index);
    rebuildItemList();
}

void FashionDressPanel::configureItemListLayout()
{
    // Heiyue: suit ScrollViewSuit colum=3 (large cards); part ScrollViewBag colum=6 (small icons).
    if (!m_activePart.has_value())
    {
        m_itemList->setDefaultItem(kSuitItemUrl);
        m_itemList->setColumnCount(3);
    }
    else
    {
        m_itemList->setDefaultItem(kPartItemUrl);
        m_itemList->setColumnCount(4);
    }
}

std::vector<const mugen::conf::FashionSuitConfig*> FashionDressPanel::suitsForOccupation() const
{
    // Heiyue DBItem:getFashionSuitList — occupation == roleType or 0; StateSuit drops shield!=0.
    std::vector<const mugen::conf::FashionSuitConfig*> out;
    const int32_t occ = occupation();
    for (const auto& suit : mugen::conf::Config::instance().table<mugen::conf::FashionSuitConfig>())
    {
        if (suit.shield != 0)
            continue;
        if (suit.occupation != 0 && suit.occupation != occ)
            continue;
        out.push_back(&suit);
    }
    std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) {
        if (a->suitQuality != b->suitQuality)
            return a->suitQuality > b->suitQuality;
        return a->id > b->id;
    });
    return out;
}

std::vector<const mugen::conf::FashionConfig*> FashionDressPanel::fashionsForPart(mugen::FashionPosition part) const
{
    // Heiyue StateSingle/getFashionTemplateMap: only current roleType rows; forever = time_type==0;
    // shield==0; one entry per fashion_template. Common (occupation 0) is NOT in the template map.
    std::vector<const mugen::conf::FashionConfig*> out;
    const int32_t occ = occupation();
    std::unordered_map<int32_t, const mugen::conf::FashionConfig*> byTemplate;
    for (const auto& row : mugen::conf::Config::instance().table<mugen::conf::FashionConfig>())
    {
        if (row.shield != 0)
            continue;
        if (row.position != static_cast<int32_t>(part))
            continue;
        if (row.occupation != occ)
            continue;
        if (row.timeType != 0)
            continue;
        const int32_t key           = row.fashionTemplate > 0 ? row.fashionTemplate : row.id;
        byTemplate.try_emplace(key, &row);
    }
    out.reserve(byTemplate.size());
    for (const auto& kv : byTemplate)
        out.push_back(kv.second);
    std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) {
        if (a->quality != b->quality)
            return a->quality > b->quality;
        return a->id > b->id;
    });
    return out;
}

void FashionDressPanel::bindFashionItem(GComponent* item, int32_t fashionId, bool selected)
{
    auto* icon = item->getChild("icon")->as<GLoader>();
    const auto* cfg = findFashion(fashionId);
    // Config already stores FairyGUI ui:// URLs.
    icon->setURL(cfg ? cfg->icon : "");
    if (auto* quality = item->getChild("loaderQuality"))
    {
        const int32_t q = cfg ? cfg->quality : 0;
        quality->as<GLoader>()->setURL(
            UiQualityRes::backgroundUrl(UiQualityRes::PlaceScenario::Fashion, q));
    }
    setItemSelectedVisual(item, selected);
    item->setData(reinterpret_cast<void*>(static_cast<intptr_t>(fashionId)));
}

void FashionDressPanel::bindSuitItem(GComponent* item, int32_t suitId, bool selected)
{
    auto* icon = item->getChild("icon")->as<GLoader>();
    const auto* suit = mugen::conf::Config::instance().findById<mugen::conf::FashionSuitConfig>(suitId);
    icon->setURL(suit ? suit->listPic : "");
    if (auto* quality = item->getChild("loaderQuality"))
    {
        const int32_t q = suit ? suit->suitQuality : 0;
        quality->as<GLoader>()->setURL(
            UiQualityRes::backgroundUrl(UiQualityRes::PlaceScenario::FashionTipQuality2, q));
    }
    if (auto* title = item->getChild("title"))
    {
        // TEXTNAME(nameId) not wired yet — show nameId until lang table is available.
        auto* titleField = title->as<GTextField>();
        titleField->setText(suit ? std::to_string(suit->nameId) : "");
        // heiyue UiFashionBagSuitItem: QUALITYTEXT(EquipNameDark, quality)
        const int32_t q = suit ? suit->suitQuality : 0;
        titleField->setColor(UiQualityRes::textColor(UiQualityRes::PlaceScenario::EquipNameDark, q));
    }
    setItemSelectedVisual(item, selected);
    item->setData(reinterpret_cast<void*>(static_cast<intptr_t>(suitId)));
}

void FashionDressPanel::rebuildItemList()
{
    configureItemListLayout();
    m_itemList->removeChildrenToPool();
    m_itemList->getItemPool()->clear();
    m_itemList->setNumItems(0);

    if (!m_activePart.has_value())
    {
        const auto suits = suitsForOccupation();
        for (const auto* suit : suits)
        {
            auto* item          = m_itemList->addItemFromPool(kSuitItemUrl)->as<GButton>();
            const bool selected = (suit->id == m_selectedSuitId);
            bindSuitItem(item, suit->id, selected);
            const int32_t suitId = suit->id;
            addClickListener(item, [this, suitId](EventContext*) { onClickSuitItem(suitId); });
        }
        AXLOGI("FashionDressPanel: suit tab items={}", suits.size());
        return;
    }

    const auto list = fashionsForPart(*m_activePart);
    for (const auto* fashion : list)
    {
        auto* item = m_itemList->addItemFromPool(kPartItemUrl)->as<GButton>();
        const bool selected =
            (fashion->id == m_selectedFashionId) ||
            (m_draftEquip.count(*m_activePart) && m_draftEquip[*m_activePart] == fashion->id);
        bindFashionItem(item, fashion->id, selected);
        const int32_t fashionId = fashion->id;
        addClickListener(item, [this, fashionId](EventContext*) { onClickFashionItem(fashionId); });
    }
    AXLOGI("FashionDressPanel: part={} items={}", static_cast<int>(*m_activePart), list.size());
}

void FashionDressPanel::refreshSelectionHighlights()
{
    for (int i = 0; i < m_itemList->numChildren(); ++i)
    {
        auto* item = m_itemList->getChildAt(i)->as<GComponent>();
        const auto id = static_cast<int32_t>(reinterpret_cast<intptr_t>(item->getData()));
        bool selected = false;
        if (!m_activePart.has_value())
            selected = (id == m_selectedSuitId);
        else
            selected = (id == m_selectedFashionId) ||
                       (m_draftEquip.count(*m_activePart) && m_draftEquip[*m_activePart] == id);
        setItemSelectedVisual(item, selected);
    }
}

void FashionDressPanel::onClickSuitItem(int32_t suitId)
{
    if (m_selectedSuitId == suitId)
        return;
    m_selectedSuitId    = suitId;
    m_selectedFashionId = 0;
    applySuit(suitId);
    refreshSelectionHighlights();
    refreshAvatar();
}

void FashionDressPanel::onClickFashionItem(int32_t fashionId)
{
    if (m_selectedFashionId == fashionId)
        return;
    m_selectedFashionId = fashionId;
    m_selectedSuitId    = 0;
    applyFashion(fashionId);
    refreshSelectionHighlights();
    refreshAvatar();
}

void FashionDressPanel::applySuit(int32_t suitId)
{
    const auto* suit = mugen::conf::Config::instance().findById<mugen::conf::FashionSuitConfig>(suitId);
    if (!suit)
        return;
    for (int32_t partId : suit->suitParts)
    {
        const auto* fashion = findFashion(partId);
        if (!fashion)
            continue;
        m_draftEquip[static_cast<mugen::FashionPosition>(fashion->position)] = fashion->id;
    }
}

void FashionDressPanel::applyFashion(int32_t fashionId)
{
    const auto* fashion = findFashion(fashionId);
    if (!fashion)
        return;
    m_draftEquip[static_cast<mugen::FashionPosition>(fashion->position)] = fashion->id;
}

void FashionDressPanel::refreshAvatar()
{
    auto* session = AppContext::get().gameSession();
    if (!session || !m_avatarLoader)
        return;

    mugen::FashionAppearance appearance;
    appearance.roleId = roleId();
    for (const auto& skin : session->selectedCharacter.defaultSkins)
        appearance.baseFashion[static_cast<mugen::FashionPosition>(skin.position)] = skin.resFashionId;
    for (const auto& kv : m_draftEquip)
        appearance.equipFashion[kv.first] = kv.second;

    // Reuse avatar via setAppearance — full create/rebuild each click freezes the UI.
    if (m_avatar)
    {
        if (m_avatar->setAppearance(appearance))
        {
            m_avatar->setMotion(m_walkPose ? "walk" : "stand", "", true);
            return;
        }
        AXLOGW("FashionDressPanel: setAppearance failed, recreating avatar");
        m_avatarLoader->setContent(nullptr);
        m_avatar = nullptr;
    }

    auto* avatar = mugen::Avatar::create(appearance);
    if (!avatar)
    {
        AXLOGW("FashionDressPanel: avatar create failed");
        return;
    }
    avatar->setMotion(m_walkPose ? "walk" : "stand", "", true);
    avatar->setAutoPlay(true);
    avatar->setPosition(m_avatarLoader->getWidth() * 0.5f, -m_avatarLoader->getHeight());
    m_avatarLoader->setContent(avatar);
    m_avatar = avatar;
}

void FashionDressPanel::saveDraftToSession()
{
    auto* session = AppContext::get().gameSession();
    if (!session)
        return;

    auto& fashions = session->selectedCharacter.fashions;

    for (const auto& kv : m_draftEquip)
    {
        const int32_t fashionId = kv.second;
        const auto pos          = static_cast<int32_t>(kv.first);
        bool found              = false;
        for (auto& f : fashions)
        {
            if (f.configID == fashionId)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            game::model::FashionModel model;
            model.id       = fashionId;
            model.configID = fashionId;
            model.position = pos;
            model.worn     = false;
            fashions.push_back(model);
        }
    }

    for (auto& f : fashions)
        f.worn = false;
    for (const auto& kv : m_draftEquip)
    {
        for (auto& f : fashions)
        {
            if (f.configID == kv.second)
            {
                f.position = static_cast<int32_t>(kv.first);
                f.worn     = true;
                break;
            }
        }
    }

    auto& inv = session->selectedInventory.fashions;
    if (!inv.empty())
    {
        for (auto& f : inv)
            f.worn = false;
        for (const auto& kv : m_draftEquip)
        {
            bool found = false;
            for (auto& f : inv)
            {
                if (f.configID == kv.second)
                {
                    f.position = static_cast<int32_t>(kv.first);
                    f.worn     = true;
                    found      = true;
                    break;
                }
            }
            if (!found)
            {
                game::model::FashionModel model;
                model.id       = kv.second;
                model.configID = kv.second;
                model.position = static_cast<int32_t>(kv.first);
                model.worn     = true;
                inv.push_back(model);
            }
        }
    }

    PB::Game::FashionWearReq req;
    for (const auto& f : fashions)
    {
        auto* out = req.add_fashions();
        out->set_id(f.id);
        out->set_config_id(f.configID);
        out->set_position(f.position);
        out->set_worn(f.worn);
    }
    FeatureProgressSync::instance().syncFashionWear(req);
    if (auto* ed = AppContext::get().eventDispatcher())
        ed->dispatch(game::GameEvent::FashionWornChanged);
}


}  // namespace gameui
