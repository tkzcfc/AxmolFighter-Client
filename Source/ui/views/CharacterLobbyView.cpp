#include "CharacterLobbyView.h"

#include "AppContext.h"
#include "TownView.h"
#include "mugen/avatar/render/Avatar.h"
#include "mugen/avatar/render/AvatarBuilder.h"
#include "mugen/avatar/render/SpineLayer.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "ui/widgets/character_lobby/CharacterCreationPanel.h"
#include "ui/widgets/common/MessagePopup.h"
#include "fairygui/GLoader3D.h"
#include <net/client_game.pb.h>

namespace gameui
{

void CharacterLobbyView::onEnter()
{
    auto startGameBtn       = this->getChild<GButton>("startGameBtn");
    auto createCharacterBtn = this->getChild<GButton>("createCharacterBtn");
    auto gameoverBtn        = this->getChild<GButton>("gameoverBtn");

    this->addClickListener(startGameBtn, AX_CALLBACK_1(CharacterLobbyView::onClickStartGameButton, this));
    this->addClickListener(createCharacterBtn, AX_CALLBACK_1(CharacterLobbyView::onClickCharacterCreateItem, this));
    this->addClickListener(gameoverBtn, AX_CALLBACK_1(CharacterLobbyView::onClickGameoverButton, this));

    requestCharacterList();
}

void CharacterLobbyView::requestCharacterList()
{
    PB::Game::FetchCharacterListReq req;
    this->call(req, [this](const PB::Game::FetchCharacterListResp* resp, std::string_view error) {
        if (!resp)
        {
            MessagePopup::showNetErr(error, [this]() { requestCharacterList(); });
            return;
        }

        if (resp->code() != 0)
        {
            const std::string msg = resp->message().empty() ? "获取角色列表失败" : resp->message();
            MessagePopup::show(msg, []() { ax::Director::getInstance()->end(); });
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
        auto item           = charactorList->getChildAt(i)->as<GButton>();
        auto nameText       = item->getChild("nameText")->as<GTextField>();
        auto professionText = item->getChild("professionText")->as<GTextField>();
        auto avatarLoader   = item->getChild("avatarLoader")->as<GLoader3D>();

        if (i >= static_cast<int>(session->characters.size()))
        {
            item->setTouchable(false);
            item->setSelected(false);
            nameText->setText("");
            professionText->setText("");
            avatarLoader->setContent(nullptr);
            continue;
        }

        const auto& c = session->characters[static_cast<size_t>(i)];
        item->setTouchable(true);
        item->setSelected(i == currentSelectedIndex);

        nameText->setText(fmt::format("Lv{} {}", c.level, c.name));
        professionText->setText("鬼剑士");

        // classID is the server role id. Keep the preview on the same
        // RoleConfig/ResSpineConfig path used by spawned actors.
        int32_t roleId = c.classID > 0 ? c.classID : 1;
        auto* config   = mugen::Config::getInstance();
        auto* role     = config->getRoleConfigById(roleId);
        if (!role)
        {
            roleId = 1;
            role   = config->getRoleConfigById(roleId);
        }
        const auto* spine = role && role->resSpineId > 0 ? config->getResSpineConfigById(role->resSpineId) : nullptr;
        mugen::SpineAvatarDesc desc;
        if (spine)
        {
            desc.skeleton    = spine->spine;
            desc.atlas       = spine->atlas;
            desc.defaultSkin = spine->defaultSkin;
            desc.scale       = spine->scale;
        }
        auto* previewAvatar = spine ? mugen::AvatarBuilder::createAvatar(desc) : nullptr;
        if (!previewAvatar)
        {
            avatarLoader->setContent(nullptr);
            continue;
        }

        previewAvatar->setMotion("stand", "", true);
        previewAvatar->setAutoPlay(true);
        previewAvatar->setPosition(avatarLoader->getWidth() * 0.5f, -avatarLoader->getHeight());
        avatarLoader->setContent(previewAvatar);
    }
}

void CharacterLobbyView::onClickStartGameButton(EventContext* context)
{
    auto* session       = AppContext::get().gameSession();
    auto* charactorList = this->getChild<GList>("charactorList");
    int selectedIndex   = charactorList->getSelectedIndex();

    if (!session || selectedIndex < 0 || selectedIndex >= static_cast<int>(session->characters.size()))
    {
        MessagePopup::show("请先选择角色");
        return;
    }

    m_selectedCharacterID        = session->characters[static_cast<size_t>(selectedIndex)].characterID;
    session->selectedCharacterID = m_selectedCharacterID;

    PB::Game::SelectCharacterReq req;
    req.set_character_id(m_selectedCharacterID);

    this->call(req, [this](const PB::Game::SelectCharacterResp* resp, std::string_view error) {
        if (!resp)
        {
            MessagePopup::showNetErr(error);
            return;
        }

        if (resp->code() != 0)
        {
            MessagePopup::show(resp->message().empty() ? "进入游戏失败" : resp->message());
            return;
        }

        if (auto* session = AppContext::get().gameSession())
        {
            session->setSelectedFromSelectResp(*resp);
        }

        getViewManager()->switchView<TownView>();
    });
}

void CharacterLobbyView::onClickCharacterCreateItem(EventContext* context)
{
    getViewManager()->getUIManager()->open<CharacterCreationPanel>();
}

void CharacterLobbyView::onClickGameoverButton(EventContext* context)
{
    ax::Director::getInstance()->end();
}

}  // namespace gameui
