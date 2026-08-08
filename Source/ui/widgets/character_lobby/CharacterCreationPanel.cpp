#include "CharacterCreationPanel.h"
#include "ui/views/CharacterLobbyView.h"
#include "ui/widgets/common/MessagePopup.h"
#include "mugen/conf/GameDef.h"
#include <net/client_game.pb.h>
#include <cstddef>

namespace gameui
{
void CharacterCreationPanel::onCreate()
{
    auto createButton = this->getChild<GButton>("createButton");
    auto backButton   = this->getChild<GButton>("backButton");

    this->addClickListener(createButton, AX_CALLBACK_1(CharacterCreationPanel::onClickCreateButton, this));
    this->addClickListener(backButton, AX_CALLBACK_1(CharacterCreationPanel::onClickBackButton, this));
}

void CharacterCreationPanel::onClickCreateButton(EventContext* context)
{
    // 职业选择列表
    auto professionList = this->getChild<GList>("professionList");
    // 角色名称输入框
    auto nameInput = this->getChild<GComponent>("nameInput")->getChild("input")->as<GTextInput>();

    // 获取选中的职业索引
    auto selectedIndex = professionList->getSelectedIndex();

    if (selectedIndex < 0)
    {
        MessagePopup::show("请选择一个职业");
        return;
    }

    // professionList 三项对应开放职业 1/2/4（不含未开放的职业 3）
    static constexpr int32_t kProfessionListToClassId[] = {
        static_cast<int32_t>(mugen::JobType::kSwordman),
        static_cast<int32_t>(mugen::JobType::kRanger),
        static_cast<int32_t>(mugen::JobType::kMage),
    };
    const auto listCount = static_cast<int>(std::size(kProfessionListToClassId));
    if (selectedIndex >= listCount)
    {
        MessagePopup::show("请选择一个职业");
        return;
    }

    const int32_t classId = kProfessionListToClassId[selectedIndex];
    if (classId != static_cast<int32_t>(mugen::JobType::kSwordman))
    {
        MessagePopup::show("职业暂未开放");
        return;
    }

    auto name = nameInput->getText();
    if (name.empty())
    {
        MessagePopup::show("请输入角色名称");
        return;
    }

    PB::Game::CreateCharacterReq req;
    req.set_name(name);
    req.set_class_id(classId);
    req.set_gender(0);

    this->call(req, [this](const PB::Game::CreateCharacterResp* resp, std::string_view error) {
        if (!resp)
        {
            MessagePopup::showNetErr(error);
            return;
        }

        if (resp->code() != 0)
        {
            MessagePopup::show(resp->message().empty() ? "创建角色失败" : resp->message());
            return;
        }

        if (auto* current = dynamic_cast<CharacterLobbyView*>(getUIManager()->getViewManager()->getCurrentView()))
        {
            current->requestCharacterList();
        }

        MessagePopup::show("创建角色成功", [this]() { this->close(); });
    });
}

void CharacterCreationPanel::onClickBackButton(EventContext* context)
{
    this->close();
}

}  // namespace gameui
