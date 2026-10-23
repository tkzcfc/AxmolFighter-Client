#include "PetMainPanel.h"

#include "PetLocalState.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/render/spine/MgSkeletonAnimation.h"
#include "fairygui/UIPackage.h"

#include <fmt/format.h>

namespace gameui
{

namespace
{
const char* kItemUrl = "ui://Panel_Pet/PetItem";

void setItemSelectedVisual(GComponent* item, bool selected)
{
    if (auto* btn = dynamic_cast<GButton*>(item))
        btn->setSelected(selected);
    if (auto* glow = item->getChild("loaderSelectGlow"))
        glow->setVisible(selected);
}
}  // namespace

GComponent* PetMainPanel::container() const
{
    return getChild<GLoader>("container")->getComponent();
}

void PetMainPanel::onCreate()
{
    addClickListener(getChild<GButton>("btnBack"), AX_CALLBACK_1(PetMainPanel::onClickBack, this));

    auto* root      = container();
    m_itemList      = root->getChild("itemList")->as<GList>();
    m_titleText     = root->getChild("titleText")->as<GTextField>();
    m_detailText    = root->getChild("detailText")->as<GTextField>();
    m_previewSpine  = root->getChild("previewSpine")->as<GLoader3D>();
    m_btnActive     = root->getChild("btnActive")->as<GButton>();
    m_btnLevel      = root->getChild("btnLevel")->as<GButton>();
    m_btnStar       = root->getChild("btnStar")->as<GButton>();

    m_titleText->setText("宠物");
    m_btnActive->setTitle("设为跟随");
    m_btnLevel->setTitle("升级");
    m_btnStar->setTitle("升星");

    addClickListener(m_btnActive, AX_CALLBACK_1(PetMainPanel::onClickActive, this));
    addClickListener(m_btnLevel, AX_CALLBACK_1(PetMainPanel::onClickLevel, this));
    addClickListener(m_btnStar, AX_CALLBACK_1(PetMainPanel::onClickStar, this));
    m_itemList->setDefaultItem(kItemUrl);
}

void PetMainPanel::onShow()
{
    PetLocalState::instance().ensureUnlocked();
    rebuildList();
    refreshDetail();
    refreshSpinePreview();
}

void PetMainPanel::onDestroy()
{
    if (m_previewSpine)
        m_previewSpine->setContent(nullptr);
    m_itemList       = nullptr;
    m_titleText      = nullptr;
    m_detailText     = nullptr;
    m_previewSpine   = nullptr;
    m_btnActive      = nullptr;
    m_btnLevel       = nullptr;
    m_btnStar        = nullptr;
    m_previewSpineId = 0;
}

void PetMainPanel::onClickBack(EventContext*)
{
    close();
}

const mugen::conf::PetConfig* PetMainPanel::findConfig(int32_t petId) const
{
    return mugen::conf::Config::instance().find<mugen::conf::PetConfig>(petId);
}

void PetMainPanel::bindItem(GComponent* item, int32_t petId, bool selected)
{
    auto* icon       = item->getChild("icon")->as<GLoader>();
    const auto* cfg  = findConfig(petId);
    const auto* inst = PetLocalState::instance().find(petId);
    icon->setURL(cfg ? cfg->icon : std::string());

    const bool isActive = PetLocalState::instance().activeId() == petId;
    if (auto* title = item->getChild("title"))
    {
        title->setText(fmt::format("#{} L{}★{}{}", petId, inst ? inst->level : 1, inst ? inst->star : 0,
                                   isActive ? "跟随" : ""));
    }
    setItemSelectedVisual(item, selected);
    item->setData(reinterpret_cast<void*>(static_cast<intptr_t>(petId)));
}

void PetMainPanel::rebuildList()
{
    const auto list = PetLocalState::instance().list();
    m_listIds.clear();
    m_listIds.reserve(list.size());
    for (const auto& st : list)
        m_listIds.push_back(st.configId);

    if (m_selectedId == 0 && !m_listIds.empty())
        m_selectedId = m_listIds.front();

    m_itemList->removeChildrenToPool();
    for (int32_t petId : m_listIds)
    {
        auto* item = m_itemList->addItemFromPool()->as<GComponent>();
        bindItem(item, petId, petId == m_selectedId);
        item->addClickListener([this, petId](EventContext*) { onClickItem(petId); });
    }
    refreshSelectionHighlights();
}

void PetMainPanel::refreshItemCells()
{
    // In-place rebind when membership is unchanged (level/star/active).
    // Avoid removeChildrenToPool — that resets scroll and flickers the list.
    const int n = m_itemList->numChildren();
    for (int i = 0; i < n; ++i)
    {
        auto* item    = m_itemList->getChildAt(i)->as<GComponent>();
        const auto id = static_cast<int32_t>(reinterpret_cast<intptr_t>(item->getData()));
        bindItem(item, id, id == m_selectedId);
    }
}

void PetMainPanel::refreshSelectionHighlights()
{
    const int n = m_itemList->numChildren();
    for (int i = 0; i < n; ++i)
    {
        auto* item    = m_itemList->getChildAt(i)->as<GComponent>();
        const auto id = static_cast<int32_t>(reinterpret_cast<intptr_t>(item->getData()));
        setItemSelectedVisual(item, id == m_selectedId);
    }
}

void PetMainPanel::refreshDetail()
{
    const auto* cfg  = findConfig(m_selectedId);
    const auto* inst = PetLocalState::instance().find(m_selectedId);
    if (!cfg || !inst)
    {
        m_detailText->setText("Select a pet");
        return;
    }

    const bool active = PetLocalState::instance().activeId() == m_selectedId;
    m_detailText->setText(fmt::format(
        "ID: {}\nnameId: {}  descId: {}\nquality: {}  rare: {}\n"
        "Level: {}/{}  Star: {}/{}\npetId: {}  spineId: {}  anim: {}\n{}\n\n"
        "All pets unlocked (dev). 设为跟随 = local active pet.",
        cfg->id, cfg->nameId, cfg->descId, cfg->quality, cfg->rare, inst->level,
        cfg->levelMax > 0 ? cfg->levelMax : PetLocalState::kMaxLevel, inst->star,
        cfg->starMax > 0 ? cfg->starMax : PetLocalState::kMaxStar, cfg->petId, cfg->spineId,
        cfg->spineAnim.empty() ? "stand1" : cfg->spineAnim,
        active ? "Status: ACTIVE follow" : "Status: not following"));
}

void PetMainPanel::refreshSpinePreview()
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

    const int32_t spineId  = cfg->spineId;
    const std::string anim = cfg->spineAnim.empty() ? "stand1" : cfg->spineAnim;
    const int32_t selected = m_selectedId;
    auto weakThis          = weak_from_this();

    mugen::MgSkeletonAnimation::createFromCacheAsync(
        spineId, [weakThis, this, spineId, anim, selected](mugen::MgSkeletonAnimation* skel) {
            if (!skel)
            {
                AXLOGW("PetMainPanel: spine load failed id={}", spineId);
                return;
            }
            if (weakThis.expired())
                return;
            if (m_selectedId != selected || m_previewSpineId != spineId || !m_previewSpine)
                return;

            skel->setPosition(ax::Vec2(m_previewSpine->getWidth() * 0.5f, 40.0f));
            skel->setScale(0.75f);
            skel->setAnimation(0, anim, true);
            m_previewSpine->setContent(skel);
            AXLOGI("PetMainPanel: preview spineId={} anim={}", spineId, anim);
        });
}

void PetMainPanel::onClickItem(int32_t petId)
{
    m_selectedId = petId;
    refreshSelectionHighlights();
    refreshDetail();
    refreshSpinePreview();
}

void PetMainPanel::onClickActive(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    auto& state = PetLocalState::instance();
    if (state.activeId() == m_selectedId)
        state.setActive(0);
    else
        state.setActive(m_selectedId);
    // Membership unchanged — rebind labels (跟随) without list rebuild.
    refreshItemCells();
    refreshDetail();
}

void PetMainPanel::onClickLevel(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    PetLocalState::instance().addLevel(m_selectedId, 1);
    refreshItemCells();
    refreshDetail();
}

void PetMainPanel::onClickStar(EventContext*)
{
    if (m_selectedId <= 0)
        return;
    PetLocalState::instance().addStar(m_selectedId, 1);
    refreshItemCells();
    refreshDetail();
}

}  // namespace gameui
