#include "CharacterLobbyView.h"

#include "AppContext.h"
#include "TownView.h"
#include "mugen/avatar/FashionSpine.h"
#include "mugen/avatar/render/Avatar.h"
#include "mugen/avatar/render/AvatarBuilder.h"
#include "mugen/common/TypeConversions.h"
#include "ui/widgets/character_lobby/CharacterCreationPanel.h"
#include "ui/widgets/common/MessageDialog.h"
#include "ui/core/AudioManager.h"
#include "fairygui/GLoader3D.h"
#include <net/client_game.pb.h>

namespace gameui
{

void CharacterLobbyView::onEnter()
{
    auto startGameBtn    = this->getChild<GButton>("startGameBtn");
    auto createPlayerBtn = this->getChild<GButton>("createPlayerBtn");

    this->addClickListener(startGameBtn, AX_CALLBACK_1(CharacterLobbyView::onClickStartGameButton, this));
    this->addClickListener(createPlayerBtn, AX_CALLBACK_1(CharacterLobbyView::onClickCreatePlayerButton, this));

    requestCharacterList();
}

void CharacterLobbyView::requestCharacterList()
{
    PB::Game::FetchCharacterListReq req;
    this->call(req, [this](const PB::Game::FetchCharacterListResp* resp, std::string_view error) {
        if (!resp)
        {
            MessageDialog::showNetErr(error, [this]() { requestCharacterList(); });
            return;
        }

        if (resp->code() != 0)
        {
            const std::string msg = resp->message().empty() ? "获取角色列表失败" : resp->message();
            MessageDialog::show(msg, []() { ax::Director::getInstance()->end(); });
            return;
        }

        if (auto* session = AppContext::get().gameSession())
        {
            session->setCharacterListFromResp(*resp);
            m_selectedCharacterID = session->selectedCharacterID;
        }

        updateCharacterList();
    });
}

void CharacterLobbyView::updateCharacterList()
{
    auto* session = AppContext::get().gameSession();
    if (!session)
    {
        return;
    }

    const int characterCount =
        session->serverConfig.maxCharacterCount > 0 ? session->serverConfig.maxCharacterCount : 10;
    auto charactorList        = this->getChild<GList>("charactorList");
    auto currentSelectedIndex = charactorList->getSelectedIndex();

    charactorList->setNumItems(characterCount);

    for (int i = 0; i < characterCount; ++i)
    {
        auto item         = charactorList->getChildAt(i)->as<GButton>();
        auto nameText     = item->getChild("nameText")->as<GTextField>();
        auto avatarLoader = item->getChild("avatarLoader")->as<GLoader3D>();

        if (i >= static_cast<int>(session->characters.size()))
        {
            item->setTouchable(false);
            item->setSelected(false);
            nameText->setText("");
            avatarLoader->setContent(nullptr);
            continue;
        }

        const auto& c = session->characters[static_cast<size_t>(i)];
        item->setTouchable(true);
        item->setSelected(i == currentSelectedIndex);

        nameText->setText(fmt::format("Lv{} {}", c.level, c.name));

        const auto cls = static_cast<mugen::CharacterClass>(c.classID);
        mugen::FashionAppearance appearance;
        appearance.roleId = mugen::type_conversions::toRoleConfigId(cls);
        for (const auto& skin : c.defaultSkins)
        {
            appearance.baseFashion[static_cast<mugen::FashionPosition>(skin.position)] = skin.resFashionId;
        }
        for (const auto& f : c.fashions)
        {
            if (f.worn && f.configID > 0)
                appearance.equipFashion[static_cast<mugen::FashionPosition>(f.position)] = f.configID;
        }
        const auto fashion = mugen::FashionResolver::resolve(appearance);

        auto* previewAvatar =
            fashion.valid ? mugen::AvatarBuilder::createAvatar(fashion) : nullptr;
        if (!previewAvatar)
        {
            AXLOGW("CharacterLobbyView: preview avatar failed classId={}", c.classID);
            avatarLoader->setContent(nullptr);
            continue;
        }

        previewAvatar->setMotion("stand", "", true);
        previewAvatar->setAutoPlay(true);
        previewAvatar->setPosition(avatarLoader->getWidth() * 0.5f, -avatarLoader->getHeight());
        avatarLoader->setContent(previewAvatar);
    }
}

void CharacterLobbyView::refreshCharacterList()
{
    updateCharacterList();
}

void CharacterLobbyView::onClickStartGameButton(EventContext* context)
{
    auto* session       = AppContext::get().gameSession();
    auto* charactorList = this->getChild<GList>("charactorList");
    int selectedIndex   = charactorList->getSelectedIndex();

    if (!session || selectedIndex < 0 || selectedIndex >= static_cast<int>(session->characters.size()))
    {
        MessageDialog::show("请先选择角色");
        return;
    }

    m_selectedCharacterID        = session->characters[static_cast<size_t>(selectedIndex)].characterID;
    session->selectedCharacterID = m_selectedCharacterID;

    PB::Game::SelectCharacterReq req;
    req.set_character_id(m_selectedCharacterID);

    this->call(req, [this](const PB::Game::SelectCharacterResp* resp, std::string_view error) {
        if (!resp)
        {
            MessageDialog::showNetErr(error);
            return;
        }

        if (resp->code() != 0)
        {
            MessageDialog::show(resp->message().empty() ? "进入游戏失败" : resp->message());
            return;
        }

        if (auto* session = AppContext::get().gameSession())
        {
            session->setSelectedFromSelectResp(*resp);
        }

        AudioManager::getInstance()->stopBGM();
        getViewManager()->switchView<TownView>();
    });
}

void CharacterLobbyView::onClickCreatePlayerButton(EventContext* context)
{
    getViewManager()->getUIManager()->open<CharacterCreationPanel>();
}

}  // namespace gameui
