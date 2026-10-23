#include "WeaponFormPreviewDialog.h"

#include "AppContext.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/GameDef.h"

namespace gameui
{

WeaponFormPreviewDialog::WeaponFormPreviewDialog(int32_t armamentSpineId) : m_armamentSpineId(armamentSpineId)
{
    m_options.hasBackground     = true;
    m_options.backgroundOpacity = 180;
    m_options.closeOnClickBg    = true;
    m_playAnimation             = false;
}

GComponent* WeaponFormPreviewDialog::onCreateContent()
{
    // Center without pivot-as-anchor so Avatar / native layers keep stable local coords
    // (createCenteredComponent uses setPivot(..., true) which blew up the prior layout).
    auto* comp = UIPackage::createObject("Panel_Weapon", "WeaponFormPreviewDialog")->as<GComponent>();
    const float x = (m_root->getWidth() - comp->getWidth()) * 0.5f;
    const float y = (m_root->getHeight() - comp->getHeight()) * 0.5f;
    comp->setPosition(x, y);
    comp->addRelation(m_root, RelationType::Center_Center);
    return comp;
}

void WeaponFormPreviewDialog::onCreate()
{
    addClickListener(getChild<GButton>("btnClose"), AX_CALLBACK_1(WeaponFormPreviewDialog::onClickClose, this));
    m_avatarLoader = getChild<GLoader3D>("loaderAvatar");
}

void WeaponFormPreviewDialog::onShow()
{
    refreshAvatar();
}

void WeaponFormPreviewDialog::onDestroy()
{
    if (m_avatarLoader)
        m_avatarLoader->setContent(nullptr);
    m_avatarLoader = nullptr;
    m_avatar       = nullptr;
}

void WeaponFormPreviewDialog::refreshAvatar()
{
    auto* session = AppContext::get().gameSession();
    if (!session || !m_avatarLoader)
        return;

    mugen::FashionAppearance appearance;
    appearance.roleId = mugen::type_conversions::toRoleConfigId(
        static_cast<mugen::CharacterClass>(session->selectedCharacter.classID));
    for (const auto& skin : session->selectedCharacter.defaultSkins)
        appearance.baseFashion[static_cast<mugen::FashionPosition>(skin.position)] = skin.resFashionId;
    for (const auto& f : session->selectedCharacter.fashions)
    {
        if (f.worn && f.configID > 0)
            appearance.equipFashion[static_cast<mugen::FashionPosition>(f.position)] = f.configID;
    }
    // Override weapon accessory with armament ResSpine id (FashionResolver treats kWeapon as ResSpine).
    if (m_armamentSpineId > 0)
        appearance.equipFashion[mugen::FashionPosition::kWeapon] = m_armamentSpineId;

    if (m_avatar)
    {
        if (m_avatar->setAppearance(appearance))
        {
            m_avatar->setMotion("stand", "", true);
            return;
        }
        m_avatarLoader->setContent(nullptr);
        m_avatar = nullptr;
    }

    auto* avatar = mugen::Avatar::create(appearance);
    if (!avatar)
    {
        AXLOGW("WeaponFormPreviewDialog: avatar create failed spineId={}", m_armamentSpineId);
        return;
    }
    avatar->setMotion("stand", "", true);
    avatar->setAutoPlay(true);
    // Mirror CharacterCreationChooseClouthPanel: feet at bottom of GLoader3D.
    avatar->setPosition(ax::Vec2(m_avatarLoader->getWidth() * 0.5f, -m_avatarLoader->getHeight()));
    avatar->setScale(0.9f);
    m_avatarLoader->setContent(avatar);
    m_avatar = avatar;
    AXLOGI("WeaponFormPreviewDialog: showing avatar with weapon spineId={}", m_armamentSpineId);
}

void WeaponFormPreviewDialog::onClickClose(EventContext*)
{
    close();
}

}  // namespace gameui
