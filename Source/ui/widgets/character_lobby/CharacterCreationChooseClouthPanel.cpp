#include "CharacterCreationChooseClouthPanel.h"
#include "ui/screens/CharacterLobbyScreen.h"
#include "ui/widgets/common/MessageDialog.h"
#include "mugen/avatar/render/Avatar.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "AppContext.h"
#include <net/client_game.pb.h>
#include "CharacterCreationPanel.h"
#include "fairygui/GLoader3D.h"

namespace gameui
{

namespace
{

const mugen::conf::CreateRoleConfig* findCreateRole(mugen::CharacterClass cls)
{
    return mugen::conf::Config::instance().find<mugen::conf::CreateRoleConfig>(static_cast<int32_t>(cls));
}

}  // namespace

bool CharacterCreationChooseClouthPanel::canSelectFashion() const
{
    const auto* row = findCreateRole(m_curClass);
    return row && row->canSelectFashion;
}

void CharacterCreationChooseClouthPanel::onCreate()
{
    auto backButton = this->getChild<GButton>("backButton");
    this->addClickListener(backButton, AX_CALLBACK_1(CharacterCreationChooseClouthPanel::onClickBackButton, this));

    auto container = this->getChild<GLoader>("container")->getComponent();

    auto* choiceHairComp     = container->getChild("choiceHair")->as<GComponent>();
    auto* choiceClothingComp = container->getChild("choiceClothing")->as<GComponent>();
    m_choiceHairList         = choiceHairComp->getChild("list")->as<GList>();
    m_choiceClothingList     = choiceClothingComp->getChild("list")->as<GList>();

    if (!canSelectFashion())
    {
        choiceHairComp->setVisible(false);
        choiceClothingComp->setVisible(false);
        if (auto* lookTitle = container->getChild("n5"))
            lookTitle->setVisible(false);
        if (auto* loaderName = container->getChild("loaderName"))
            loaderName->setVisible(false);
    }
    else
    {
        m_choiceHairList->addEventListener(UIEventType::Scroll,
                                           [this](EventContext*) { this->m_choiceHairListScrollEnd = false; });
        m_choiceClothingList->addEventListener(UIEventType::Scroll,
                                               [this](EventContext*) { this->m_choiceClothingListScrollEnd = false; });
        m_choiceHairList->addEventListener(UIEventType::ScrollEnd, [this](EventContext*) {
            this->m_choiceHairListScrollEnd = true;
            this->updateUI();
        });
        m_choiceClothingList->addEventListener(UIEventType::ScrollEnd, [this](EventContext*) {
            this->m_choiceClothingListScrollEnd = true;
            this->updateUI();
        });

        container->getController("c1")->setSelectedIndex(static_cast<int>(m_curClass) - 1);
    }

    m_inputTextName = container->getChild("inputTextName")->as<GTextInput>();

    auto createButton = container->getChild("createButton")->as<GButton>();
    this->addClickListener(createButton, AX_CALLBACK_1(CharacterCreationChooseClouthPanel::onClickCreateButton, this));
}

void CharacterCreationChooseClouthPanel::onShow()
{
    if (!canSelectFashion())
    {
        updateUI();
        return;
    }

    const auto* row = findCreateRole(m_curClass);
    if (!row)
    {
        AXLOGW("CharacterCreationChooseClouthPanel: create_role row missing for class {}",
               static_cast<int>(m_curClass));
        return;
    }

    m_choiceHairList->itemRenderer = [this](int index, GObject* obj) {
        const auto* lookRow = findCreateRole(m_curClass);
        if (!lookRow || index < 0 || static_cast<size_t>(index) >= lookRow->hairLooks.size())
            return;
        obj->setIcon(lookRow->hairLooks[static_cast<size_t>(index)].icon);
    };
    m_choiceHairList->setVirtualAndLoop();
    m_choiceHairList->setNumItems(static_cast<int>(row->hairLooks.size()));
    m_choiceHairList->addEventListener(UIEventType::Scroll,
                                       AX_CALLBACK_1(CharacterCreationChooseClouthPanel::doSpecialEffect, this));

    m_choiceClothingList->itemRenderer = [this](int index, GObject* obj) {
        const auto* lookRow = findCreateRole(m_curClass);
        if (!lookRow || index < 0 || static_cast<size_t>(index) >= lookRow->clothLooks.size())
            return;
        obj->setIcon(lookRow->clothLooks[static_cast<size_t>(index)].icon);
    };
    m_choiceClothingList->setVirtualAndLoop();
    m_choiceClothingList->setNumItems(static_cast<int>(row->clothLooks.size()));
    m_choiceClothingList->addEventListener(UIEventType::Scroll,
                                           AX_CALLBACK_1(CharacterCreationChooseClouthPanel::doSpecialEffect, this));

    doSpecialEffect(nullptr);
}

void CharacterCreationChooseClouthPanel::onDestroy() {}

void CharacterCreationChooseClouthPanel::updateUI()
{
    auto container    = this->getChild<GLoader>("container")->getComponent();
    auto loaderAvatar = container->getChild("loaderAvatar")->as<GLoader3D>();

    mugen::FashionAppearance appearance;
    appearance.roleId = mugen::type_conversions::toRoleConfigId(m_curClass);

    if (canSelectFashion())
    {
        if (!m_choiceHairListScrollEnd || !m_choiceClothingListScrollEnd)
            return;

        if (m_curHairIndex == -1 || m_curClothingIndex == -1)
            return;

        if (m_showHairIndex == m_curHairIndex && m_showClothingIndex == m_curClothingIndex)
            return;

        const auto* row = findCreateRole(m_curClass);
        if (!row || static_cast<size_t>(m_curHairIndex) >= row->hairLooks.size() ||
            static_cast<size_t>(m_curClothingIndex) >= row->clothLooks.size())
            return;

        m_showHairIndex     = m_curHairIndex;
        m_showClothingIndex = m_curClothingIndex;

        auto t0 = container->getTransition("t0");
        if (t0 && loaderAvatar->getContent() != nullptr)
        {
            t0->stop();
            t0->play();
        }

        appearance.baseFashion[mugen::FashionPosition::kHair]    = row->hairLooks[static_cast<size_t>(m_curHairIndex)].id;
        appearance.baseFashion[mugen::FashionPosition::kClothes] =
            row->clothLooks[static_cast<size_t>(m_curClothingIndex)].id;
        appearance.baseFashion[mugen::FashionPosition::kSkin] =
            row->clothLooks[static_cast<size_t>(m_curClothingIndex)].skinId;
    }

    if (auto* preview = dynamic_cast<mugen::Avatar*>(loaderAvatar->getContent()))
    {
        preview->setAppearance(appearance);
        return;
    }

    auto* preview = mugen::Avatar::create(appearance);
    if (!preview)
    {
        AXLOGW("CharacterCreationChooseClouthPanel: fashion avatar failed class={} hair={} clothes={}",
               static_cast<int>(m_curClass), m_curHairIndex, m_curClothingIndex);
        return;
    }
    preview->setMotion("stand", "", true);
    preview->setAutoPlay(true);
    preview->setPosition(ax::Vec2(loaderAvatar->getWidth() * 0.5f, -loaderAvatar->getHeight()));
    loaderAvatar->setContent(preview);
}

void CharacterCreationChooseClouthPanel::doSpecialEffect(EventContext* context)
{
    if (m_choiceHairList->getNumItems() <= 0 || m_choiceClothingList->getNumItems() <= 0)
        return;

    int32_t nextHairIndex   = (m_choiceHairList->getFirstChildInView() + 1) % m_choiceHairList->getNumItems();
    int32_t nextClouthIndex = (m_choiceClothingList->getFirstChildInView() + 1) % m_choiceClothingList->getNumItems();

    adjustListItem(m_choiceHairList, nextHairIndex);
    adjustListItem(m_choiceClothingList, nextClouthIndex);

    if (nextHairIndex != m_curHairIndex || nextClouthIndex != m_curClothingIndex)
    {
        m_curHairIndex     = nextHairIndex;
        m_curClothingIndex = nextClouthIndex;
        this->updateUI();
    }
}

void CharacterCreationChooseClouthPanel::adjustListItem(GList* list, int32_t curIndex)
{
    int cnt = list->numChildren();
    for (int i = 0; i < cnt; i++)
    {
        GButton* obj = list->getChildAt(i)->as<GButton>();
        obj->setSelected(i == curIndex);
    }
}

void CharacterCreationChooseClouthPanel::onClickCreateButton(EventContext* context)
{
    const auto* row            = findCreateRole(m_curClass);
    const bool selectFashion   = canSelectFashion();
    if (selectFashion && (m_curHairIndex < 0 || m_curClothingIndex < 0 || !row ||
                          static_cast<size_t>(m_curHairIndex) >= row->hairLooks.size() ||
                          static_cast<size_t>(m_curClothingIndex) >= row->clothLooks.size()))
    {
        MessageDialog::show("请选择头饰和服饰");
        return;
    }

    const std::string name = m_inputTextName->getText();
    if (name.empty())
    {
        MessageDialog::show("请输入角色名");
        return;
    }

    PB::Game::CreateCharacterReq req;
    req.set_name(name);
    req.set_class_id(static_cast<int32_t>(m_curClass));
    req.set_gender(0);
    req.set_hair_id(selectFashion ? row->hairLooks[static_cast<size_t>(m_curHairIndex)].id : 0);
    req.set_clothes_id(selectFashion ? row->clothLooks[static_cast<size_t>(m_curClothingIndex)].id : 0);

    this->call(req, [this](const PB::Game::CreateCharacterResp* resp, std::string_view error) {
        if (!resp)
        {
            MessageDialog::showNetErr(error);
            return;
        }
        if (resp->code() != 0)
        {
            MessageDialog::show(resp->message().empty() ? "创建角色失败" : resp->message());
            return;
        }

        if (auto* session = AppContext::get().gameSession())
            session->appendFromCreateResp(*resp);

        if (auto* lobby = dynamic_cast<CharacterLobbyScreen*>(getUIManager()->getScreenManager()->getCurrentScreen()))
        {
            lobby->refreshCharacterList();
        }

        this->close();
    });
}

void CharacterCreationChooseClouthPanel::onClickBackButton(EventContext* context)
{
    this->getUIManager()->open<CharacterCreationPanel>();
    this->close();
}

}  // namespace gameui
