#include "CharacterCreationPanel.h"
#include "ui/views/CharacterLobbyView.h"
#include "ui/widgets/common/MessageDialog.h"
#include "ui/UiConfig.h"
#include "mugen/render/spine/SpineSkeletonLoader.h"
#include "mugen/conf/GameDef.h"
#include "ui/core/AudioManager.h"
#include <net/client_game.pb.h>
#include "CharacterCreationChooseClouthPanel.h"

#include <cstring>

namespace gameui
{

void CharacterCreationPanel::onCreate()
{
    AudioManager::getInstance()->pauseBGM();

    auto backButton = this->getChild<GButton>("backButton");
    this->addClickListener(backButton, AX_CALLBACK_1(CharacterCreationPanel::onClickBackButton, this));

    auto container = this->getChild<GComponent>("container");

    // 职业选择列表
    m_roleTypeSelector = container->getChild("roleTypeSelector")->as<GComponent>();
    for (int32_t i = 0; i < kOpenCharacterCount; ++i)
    {
        auto btn = m_roleTypeSelector->getChild("n" + std::to_string(i))->as<GButton>();
        this->addClickListener(btn, [this, i](EventContext* context) { this->onClickRoleTypeButton(context, i); });
    }

    for (int32_t i = 0; i < kProfessionCount; ++i)
    {
        auto professionButton = container->getChild("professionButton" + std::to_string(i))->as<GButton>();
        this->addClickListener(professionButton,
                               [this, i](EventContext* context) { this->onClickProfessionButton(context, i); });
    }

    // 创建角色按钮
    auto createButton = container->getChild("createButton")->as<GButton>();
    this->addClickListener(createButton, AX_CALLBACK_1(CharacterCreationPanel::onClickCreateButton, this));

    this->updateUI();
}

void CharacterCreationPanel::onDestroy()
{
    AudioManager::getInstance()->resumeBGM();
}

void CharacterCreationPanel::updateUI()
{
    for (int32_t i = 0; i < kOpenCharacterCount; ++i)
    {
        auto btn = m_roleTypeSelector->getChild("n" + std::to_string(i))->as<GButton>();
        btn->setTouchable(i != m_curRoleTypeIndex);
        btn->setSelected(i == m_curRoleTypeIndex);
    }
    auto& curRoleConfig       = kRoleIntros[m_curRoleTypeIndex];
    auto& curProfessionConfig = curRoleConfig.professions[m_curProfessionIndex];
    auto container            = this->getChild<GComponent>("container");

    for (int32_t i = 0; i < kProfessionCount; ++i)
    {
        auto professionButton = container->getChild("professionButton" + std::to_string(i))->as<GButton>();
        professionButton->setTouchable(i != m_curProfessionIndex);
        professionButton->setSelected(i == m_curProfessionIndex);
        professionButton->setText(curRoleConfig.professions[i].name);
    }

    // 属性描述图片
    auto loaderAttr = container->getChild("loaderAttr")->as<GLoader>();
    loaderAttr->setURL(curRoleConfig.attributeIcon);

    // 职业名称
    auto loaderName = container->getChild("loaderName")->as<GLoader>();
    loaderName->setURL(curProfessionConfig.icon);

    // 职业描述
    auto textDesc = container->getChild("textDesc")->as<GTextField>();
    textDesc->setText(curProfessionConfig.description);

    // 操作难度
    auto cStar = container->getController("c_star");
    cStar->setSelectedIndex(curProfessionConfig.difficulty);

    // 职业模型
    auto loaderAvatar = container->getChild("loaderAvatar")->as<GLoader3D>();

    const auto& spine          = curProfessionConfig.spine;
    auto skeletonAnimation     = mugen::SpineSkeletonLoader::createSkeletonAnimation(spine.id);
    skeletonAnimation->setPosition(ax::Vec2(loaderAvatar->getWidth() * 0.5f, -loaderAvatar->getHeight()) +
                                   ax::Vec2(spine.offsetX, spine.offsetY));
    skeletonAnimation->setScale(spine.scaleX, spine.scaleY);
    // 有出现动画
    if (m_curProfessionIndex == 0)
    {
        std::string animationName = spine.animationName;

        skeletonAnimation->setAnimation(0, "chuxian", false);
        skeletonAnimation->setCompleteListener([skeletonAnimation, animationName](const mugen::MgTrackEntry& entry) {
            if (entry.animationName() && std::strcmp(entry.animationName(), "chuxian") == 0)
            {
                skeletonAnimation->setAnimation(0, animationName, true);
                skeletonAnimation->setCompleteListener(nullptr);
            }
        });
    }
    else
    {
        skeletonAnimation->setAnimation(0, spine.animationName, true);
    }

    loaderAvatar->setContent(skeletonAnimation);

    // 视频
    if (m_mediaPlayer == nullptr)
    {
        auto loaderVedio = container->getChild("loaderVedio")->as<GLoader3D>();
        m_mediaPlayer    = ax::ui::MediaPlayer::create();
        m_mediaPlayer->setContentSize(loaderVedio->getSize());
        m_mediaPlayer->setPosition(ax::Vec2(loaderVedio->getWidth() * 0.5f, -loaderVedio->getHeight() * 0.5f));
        m_mediaPlayer->setLooping(true);
        loaderVedio->setContent(m_mediaPlayer);
    }
    m_mediaPlayer->setURL(curProfessionConfig.video);
    m_mediaPlayer->play();
}

void CharacterCreationPanel::onClickCreateButton(EventContext* context)
{
    getUIManager()->open<CharacterCreationChooseClouthPanel>(static_cast<mugen::CharacterClass>(m_curRoleTypeIndex + 1));
    this->close();
}

void CharacterCreationPanel::onClickBackButton(EventContext* context)
{
    this->close();
}

void CharacterCreationPanel::onClickRoleTypeButton(EventContext* context, int32_t index)
{
    if (index >= kOpenCharacterCount || (index + 1) == mugen::CharacterClass::kFighter)
    {
        MessageDialog::show("敬请期待");
        return;
    }

    if (m_curRoleTypeIndex == index)
        return;

    m_curRoleTypeIndex   = index;
    m_curProfessionIndex = 0;
    updateUI();
}

void CharacterCreationPanel::onClickProfessionButton(EventContext* context, int32_t index)
{
    if (m_curProfessionIndex == index)
        return;
    m_curProfessionIndex = index;
    updateUI();
}

}  // namespace gameui
