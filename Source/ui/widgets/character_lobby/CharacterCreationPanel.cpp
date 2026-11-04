#include "CharacterCreationPanel.h"
#include "ui/widgets/common/MessageDialog.h"
#include "mugen/render/spine/MgSkeletonAnimation.h"
#include "mugen/conf/ReflectedConfig.h"
#include "core/audio/AudioManager.h"
#include "CharacterCreationChooseClouthPanel.h"
#include "fairygui/GLoader3D.h"

#include <cstring>

namespace gameui
{

namespace
{

constexpr int32_t kRoleTypeCount = mugen::CharacterClass::kCount;

int32_t classIdFromIndex(int32_t index)
{
    return index + 1;
}

const mugen::conf::CreateRoleConfig* findCreateRole(int32_t classId)
{
    return mugen::conf::Config::instance().find<mugen::conf::CreateRoleConfig>(classId);
}

bool isCreateRoleEnabled(int32_t classId)
{
    const auto* row = findCreateRole(classId);
    return row && row->enabled;
}

}  // namespace

void CharacterCreationPanel::onCreate()
{
    game::AudioManager::getInstance()->pauseBGM();

    auto backButton = this->getChild<GButton>("backButton");
    this->addClickListener(backButton, AX_CALLBACK_1(CharacterCreationPanel::onClickBackButton, this));

    auto container = this->getChild<GLoader>("container")->getComponent();

    m_roleTypeSelector = container->getChild("roleTypeSelector")->as<GComponent>();
    for (int32_t i = 0; i < kRoleTypeCount; ++i)
    {
        auto* child                               = m_roleTypeSelector->getChild("n" + std::to_string(i));
        auto* btn                                 = dynamic_cast<GButton*>(child);
        m_roleTypeButtons[static_cast<size_t>(i)] = btn;
        // 不存在的按钮则表示该职业暂未开放，忽略
        if (!btn)
        {
            AXLOGW("CharacterCreationPanel: hero role button n{} is missing in UI package", i);
            continue;
        }
        this->addClickListener(btn, [this, i](EventContext* context) { this->onClickRoleTypeButton(context, i); });
    }

    // 创建角色按钮
    auto createButton = container->getChild("createButton")->as<GButton>();
    this->addClickListener(createButton, AX_CALLBACK_1(CharacterCreationPanel::onClickCreateButton, this));

    this->updateUI();
}

void CharacterCreationPanel::onDestroy()
{
    game::AudioManager::getInstance()->resumeBGM();
}

void CharacterCreationPanel::updateUI()
{
    for (int32_t i = 0; i < kRoleTypeCount; ++i)
    {
        auto* btn = m_roleTypeButtons[static_cast<size_t>(i)];
        if (!btn || !btn->isVisible())
            continue;
        btn->setTouchable(i != m_curRoleTypeIndex);
        btn->setSelected(i == m_curRoleTypeIndex);
    }

    const int32_t classId = classIdFromIndex(m_curRoleTypeIndex);
    const auto* row       = findCreateRole(classId);
    if (!row)
    {
        AXLOGW("CharacterCreationPanel: create_role row missing for class {}", classId);
        return;
    }

    auto container = this->getChild<GLoader>("container")->getComponent();

    if (auto* awakenLabel = container->getChild("n3"))
        awakenLabel->setVisible(false);

    for (int32_t i = 0; i < 3; ++i)
    {
        if (auto* professionButton = container->getChild("professionButton" + std::to_string(i)))
            professionButton->setVisible(false);
    }

    auto textName = container->getChild("textName")->as<GTextField>();
    textName->setText(row->name);

    auto textDesc = container->getChild("textDesc")->as<GTextField>();
    textDesc->setText(row->description);

    auto cStar = container->getController("c_star");
    cStar->setSelectedIndex(row->difficulty);

    auto loaderAvatar = container->getChild("loaderAvatar")->as<GLoader3D>();
    loaderAvatar->setContent(nullptr);

    auto weakThis = this->weak_from_this();
    mugen::MgSkeletonAnimation::createFromCacheAsync(
        row->previewSpineId,
        [weakThis, this, loaderAvatar, roleTypeIndex = m_curRoleTypeIndex](mugen::MgSkeletonAnimation* skeletonAnimation) {
        if (!skeletonAnimation)
        {
            AXLOGW("CharacterCreationPanel: async profession spine failed");
            return;
        }
        if (weakThis.expired())
            return;
        if (roleTypeIndex != m_curRoleTypeIndex)
            return;

        const auto* loadedRow = findCreateRole(classIdFromIndex(m_curRoleTypeIndex));
        if (!loadedRow)
            return;

        skeletonAnimation->setPosition(ax::Vec2(loaderAvatar->getWidth() * 0.5f, -loaderAvatar->getHeight()) +
                                       ax::Vec2(loadedRow->previewOffsetX.toFloatForRender(),
                                                loadedRow->previewOffsetY.toFloatForRender()));
        skeletonAnimation->setScale(loadedRow->previewScaleX.toFloatForRender(),
                                    loadedRow->previewScaleY.toFloatForRender());
        if (skeletonAnimation->findAnimation("chuxian"))
        {
            std::string animationName = loadedRow->previewAnim;
            skeletonAnimation->setAnimation(0, "chuxian", false);
            skeletonAnimation->setCompleteListener(
                [skeletonAnimation, animationName](const mugen::MgTrackEntry& entry) {
                if (entry.animationName() && std::strcmp(entry.animationName(), "chuxian") == 0)
                {
                    skeletonAnimation->setAnimation(0, animationName, true);
                    skeletonAnimation->setCompleteListener(nullptr);
                }
            });
        }
        else
        {
            skeletonAnimation->setAnimation(0, loadedRow->previewAnim, true);
        }

        loaderAvatar->setContent(skeletonAnimation);
    });
}

void CharacterCreationPanel::onClickCreateButton(EventContext* context)
{
    const int32_t classId = classIdFromIndex(m_curRoleTypeIndex);
    if (!isCreateRoleEnabled(classId))
    {
        MessageDialog::show("敬请期待");
        return;
    }

    getUIManager()->open<CharacterCreationChooseClouthPanel>(static_cast<mugen::CharacterClass>(classId));
    this->close();
}

void CharacterCreationPanel::onClickBackButton(EventContext* context)
{
    this->close();
}

void CharacterCreationPanel::onClickRoleTypeButton(EventContext* context, int32_t index)
{
    if (index < 0 || index >= kRoleTypeCount || !isCreateRoleEnabled(classIdFromIndex(index)))
    {
        MessageDialog::show("敬请期待");
        return;
    }

    if (m_curRoleTypeIndex == index)
        return;

    m_curRoleTypeIndex = index;
    updateUI();
}

}  // namespace gameui
