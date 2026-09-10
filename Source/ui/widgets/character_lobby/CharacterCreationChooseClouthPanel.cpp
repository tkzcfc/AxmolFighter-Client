#include "CharacterCreationChooseClouthPanel.h"
#include "ui/views/CharacterLobbyView.h"
#include "ui/widgets/common/MessageDialog.h"
#include "ui/UiConfig.h"
#include "mugen/avatar/FashionSpine.h"
#include "mugen/avatar/render/Avatar.h"
#include "mugen/avatar/render/AvatarBuilder.h"
#include "mugen/common/TypeConversions.h"
#include "AppContext.h"
#include "ui/core/AudioManager.h"
#include <net/client_game.pb.h>
#include "CharacterCreationPanel.h"
#include "fairygui/GLoader3D.h"

namespace gameui
{

void CharacterCreationChooseClouthPanel::onCreate()
{
    auto backButton = this->getChild<GButton>("backButton");
    this->addClickListener(backButton, AX_CALLBACK_1(CharacterCreationChooseClouthPanel::onClickBackButton, this));

    auto container = this->getChild<GComponent>("container");

    m_choiceHairList     = container->getChild("choiceHair")->as<GComponent>()->getChild("list")->as<GList>();
    m_choiceClothingList = container->getChild("choiceClothing")->as<GComponent>()->getChild("list")->as<GList>();

        m_choiceHairList->addEventListener(UIEventType::Scroll, [this](EventContext*) {
        this->m_choiceHairListScrollEnd = false;
    });
    m_choiceClothingList->addEventListener(UIEventType::Scroll,
                                               [this](EventContext*) { this->m_choiceClothingListScrollEnd = false; });
    m_choiceHairList->addEventListener(UIEventType::ScrollEnd,
                                       [this](EventContext*) { this->m_choiceHairListScrollEnd = true;
        this->updateUI();
        });
    m_choiceClothingList->addEventListener(UIEventType::ScrollEnd, [this](EventContext*) {
        this->m_choiceClothingListScrollEnd = true;
        this->updateUI();
    });

    m_inputTextName = container->getChild("inputTextName")->as<GTextInput>();

    // 创建角色按钮
    auto createButton = container->getChild("createButton")->as<GButton>();
    this->addClickListener(createButton, AX_CALLBACK_1(CharacterCreationChooseClouthPanel::onClickCreateButton, this));

    // 根据当前职业设置选择器的页签
    container->getController("c1")->setSelectedIndex(static_cast<int>(m_curClass) - 1);
}

void CharacterCreationChooseClouthPanel::onShow()
{
    m_choiceHairList->itemRenderer = [this](int index, GObject* obj) {
        obj->setIcon(kLookIcons[this->m_curClass - 1].hairIcons[index]);
    };
    m_choiceHairList->setVirtualAndLoop();
    m_choiceHairList->setNumItems(kCreateRoleVariantCount);
    m_choiceHairList->addEventListener(UIEventType::Scroll,
                                       AX_CALLBACK_1(CharacterCreationChooseClouthPanel::doSpecialEffect, this));

    m_choiceClothingList->itemRenderer = [this](int index, GObject* obj) {
        obj->setIcon(kLookIcons[this->m_curClass - 1].clothIcons[index]);
    };
    m_choiceClothingList->setVirtualAndLoop();
    m_choiceClothingList->setNumItems(kCreateRoleVariantCount);
    m_choiceClothingList->addEventListener(UIEventType::Scroll,
                                       AX_CALLBACK_1(CharacterCreationChooseClouthPanel::doSpecialEffect, this));

    doSpecialEffect(nullptr);
}

void CharacterCreationChooseClouthPanel::onDestroy()
{
}

void CharacterCreationChooseClouthPanel::updateUI()
{
    // 正在滚动中,不更新UI
    if (!m_choiceHairListScrollEnd || !m_choiceClothingListScrollEnd)
        return;

    // 当前选择的索引无效
    if (m_curHairIndex == -1 || m_curClothingIndex == -1)
        return;

    // 当前显示的索引与当前选择的索引相同,不更新UI
    if (m_showHairIndex == m_curHairIndex && m_showClothingIndex == m_curClothingIndex)
        return;

    m_showHairIndex = m_curHairIndex;
    m_showClothingIndex = m_curClothingIndex;

    auto container     = this->getChild<GComponent>("container");
    auto loaderAvatar = container->getChild("loaderAvatar")->as<GLoader3D>();

    auto t0 = container->getTransition("t0");
    if (t0 && loaderAvatar->getContent() != nullptr)
    {
        t0->stop();
        t0->play();
    }

    mugen::FashionAppearance appearance;
    appearance.roleId = mugen::type_conversions::toRoleConfigId(m_curClass);
    const auto& look  = kLookIcons[static_cast<int>(m_curClass) - 1];
    appearance.baseFashion[mugen::FashionPosition::kHair]    = look.hairIds[m_curHairIndex];
    appearance.baseFashion[mugen::FashionPosition::kClothes] = look.clothIds[m_curClothingIndex];
    appearance.baseFashion[mugen::FashionPosition::kSkin]    = look.skinIds[m_curClothingIndex];
    auto fashion          = mugen::FashionResolver::resolve(appearance);
    auto* preview = fashion.valid ? mugen::AvatarBuilder::createFashionAvatar(fashion) : nullptr;
    if (!preview)
    {
        AXLOGW("CharacterCreationChooseClouthPanel: fashion avatar failed class={} hair={} clothes={}",
               static_cast<int>(m_curClass), m_curHairIndex, m_curClothingIndex);
        loaderAvatar->setContent(nullptr);
        return;
    }
    preview->setMotion("stand", "", true);
    preview->setAutoPlay(true);
    preview->setPosition(ax::Vec2(loaderAvatar->getWidth() * 0.5f, -loaderAvatar->getHeight()));
    loaderAvatar->setContent(preview);
}

void CharacterCreationChooseClouthPanel::doSpecialEffect(EventContext* context)
{
    int32_t nextHairIndex   = (m_choiceHairList->getFirstChildInView() + 1) % m_choiceHairList->getNumItems();
    int32_t nextClouthIndex = (m_choiceClothingList->getFirstChildInView() + 1) % m_choiceClothingList->getNumItems();

    adjustListItem(m_choiceHairList, nextHairIndex);
    adjustListItem(m_choiceClothingList, nextClouthIndex);

    if (nextHairIndex != m_curHairIndex || nextClouthIndex != m_curClothingIndex)
    {
        m_curHairIndex = nextHairIndex;
        m_curClothingIndex = nextClouthIndex;
        this->updateUI();
    }
}

void CharacterCreationChooseClouthPanel::adjustListItem(GList* list, int32_t curIndex)
{
    float midX = list->getScrollPane()->getPosX() + list->getViewWidth() / 2;
    int cnt    = list->numChildren();
    for (int i = 0; i < cnt; i++)
    {
        GButton* obj = list->getChildAt(i)->as<GButton>();
        obj->setSelected(i == curIndex);
    }
}

void CharacterCreationChooseClouthPanel::onClickCreateButton(EventContext* context)
{
    if (m_curHairIndex < 0 || m_curClothingIndex < 0)
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
    req.set_hair_id(kLookIcons[m_curClass - 1].hairIds[m_curHairIndex]);
    req.set_clothes_id(kLookIcons[m_curClass - 1].clothIds[m_curClothingIndex]);

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

        if (auto* lobby =
                dynamic_cast<CharacterLobbyView*>(getUIManager()->getViewManager()->getCurrentView()))
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
