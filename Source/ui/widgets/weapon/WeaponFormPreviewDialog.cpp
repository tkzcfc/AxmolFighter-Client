#include "WeaponFormPreviewDialog.h"

#include "AppContext.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/GameDef.h"

namespace gameui
{

WeaponFormPreviewDialog::WeaponFormPreviewDialog(int32_t armamentSpineId, std::string spineAnim)
    : m_armamentSpineId(armamentSpineId), m_spineAnim(std::move(spineAnim))
{
    m_options.hasBackground     = true;
    m_options.backgroundOpacity = 180;
    m_options.closeOnClickBg    = true;
    m_playAnimation             = false;
    if (m_spineAnim.empty())
        m_spineAnim = "stand";
}

GComponent* WeaponFormPreviewDialog::onCreateContent()
{
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
    refreshPreview();
}

void WeaponFormPreviewDialog::onDestroy()
{
    clearPreview();
    m_avatarLoader = nullptr;
}

void WeaponFormPreviewDialog::clearPreview()
{
    if (m_avatarLoader)
        m_avatarLoader->setContent(nullptr);
    m_avatar         = nullptr;
    m_artifactSpine  = nullptr;
}

void WeaponFormPreviewDialog::refreshPreview()
{
    auto* session = AppContext::get().gameSession();
    if (!session || !m_avatarLoader)
        return;

    clearPreview();

    // Body fashion only — force occupational hand-weapon off (0). FashionResolver.merge keeps
    // explicit 0 instead of injecting EQUIPMENT_DEFAULT (e.g. lilimu gun 14301).
    mugen::FashionAppearance appearance;
    appearance.roleId = mugen::type_conversions::toRoleConfigId(
        static_cast<mugen::CharacterClass>(session->selectedCharacter.classID));
    for (const auto& skin : session->selectedCharacter.defaultSkins)
    {
        const auto pos = static_cast<mugen::FashionPosition>(skin.position);
        if (pos == mugen::FashionPosition::kWeapon)
            continue;
        appearance.baseFashion[pos] = skin.resFashionId;
    }
    for (const auto& f : session->selectedCharacter.fashions)
    {
        if (!f.worn || f.configID <= 0)
            continue;
        const auto pos = static_cast<mugen::FashionPosition>(f.position);
        if (pos == mugen::FashionPosition::kWeapon)
            continue;
        appearance.equipFashion[pos] = f.configID;
    }
    appearance.baseFashion[mugen::FashionPosition::kWeapon] = 0;
    appearance.equipFashion.erase(mugen::FashionPosition::kWeapon);

    auto* avatar = mugen::Avatar::create(appearance);
    if (!avatar)
    {
        AXLOGW("WeaponFormPreviewDialog: avatar create failed");
        return;
    }
    avatar->setMotion("stand", "", true);
    avatar->setAutoPlay(true);
    // Heiyue ARM_PREVIEW: role on the left; feet at loader bottom.
    const float loaderW = m_avatarLoader->getWidth();
    const float loaderH = m_avatarLoader->getHeight();
    avatar->setPosition(ax::Vec2(loaderW * 0.32f, -loaderH));
    avatar->setScale(0.9f);

    // Host both role + artifact under one node so GLoader3D keeps them together.
    auto* host = ax::Node::create();
    host->addChild(avatar);
    m_avatar = avatar;

    if (m_armamentSpineId > 0)
    {
        // Mirror heiyue CreateUIRoleWeapon / ROLE_WEAPON: artifact spine as sibling, not kWeapon fashion.
        const int32_t spineId = m_armamentSpineId;
        const std::string anim = m_spineAnim;
        auto weakThis          = weak_from_this();
        mugen::MgSkeletonAnimation::createFromCacheAsync(
            spineId, [weakThis, this, host, spineId, anim, loaderW, loaderH](mugen::MgSkeletonAnimation* skel) {
                if (!skel)
                {
                    AXLOGW("WeaponFormPreviewDialog: artifact spine load failed id={}", spineId);
                    return;
                }
                if (weakThis.expired() || !m_avatarLoader || m_armamentSpineId != spineId)
                    return;
                skel->setAnimation(0, anim, true);
                // Place to the right of the role (companion / 神器), heiyue NodePlayer layout.
                skel->setPosition(ax::Vec2(loaderW * 0.72f, -loaderH * 0.15f));
                skel->setScale(0.85f);
                host->addChild(skel);
                m_artifactSpine = skel;
                AXLOGI("WeaponFormPreviewDialog: artifact spineId={} anim={} (sibling, not hand weapon)", spineId,
                       anim);
            });
    }

    m_avatarLoader->setContent(host);
    AXLOGI("WeaponFormPreviewDialog: role without default weapon; artifactSpineId={}", m_armamentSpineId);
}

void WeaponFormPreviewDialog::onClickClose(EventContext*)
{
    close();
}

}  // namespace gameui
