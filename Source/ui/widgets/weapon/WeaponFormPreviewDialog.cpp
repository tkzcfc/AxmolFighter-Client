#include "WeaponFormPreviewDialog.h"

#include "AppContext.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/GameDef.h"

namespace gameui
{

namespace
{
// Heiyue ConstUI.EQUIPMENT_ZORDER: swordman/fighter/mage -1, ranger +1.
int artifactLocalZOrder(mugen::CharacterClass cls)
{
    return cls == mugen::CharacterClass::kRanger ? 1 : -1;
}
}  // namespace

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
    m_avatar        = nullptr;
    m_artifactSpine = nullptr;
}

void WeaponFormPreviewDialog::refreshPreview()
{
    auto* session = AppContext::get().gameSession();
    if (!session || !m_avatarLoader)
        return;

    clearPreview();

    const auto cls = static_cast<mugen::CharacterClass>(session->selectedCharacter.classID);

    // Body fashion only — force occupational hand-weapon off (0).
    mugen::FashionAppearance appearance;
    appearance.roleId = mugen::type_conversions::toRoleConfigId(cls);
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

    // GLoader3D content space: y=0 at top, y=-height at feet line (same as ChooseClouth).
    // Center the composed role+神器 on the ground line — heiyue NodePlayer origin.
    const float loaderW = m_avatarLoader->getWidth();
    const float loaderH = m_avatarLoader->getHeight();
    avatar->setPosition(ax::Vec2(loaderW * 0.5f, -loaderH));
    avatar->setScale(0.85f);
    m_avatarLoader->setContent(avatar);
    m_avatar = avatar;

    if (m_armamentSpineId <= 0)
        return;

    // Heiyue CreateUIRoleWeapon: ROLE_WEAPON is a child of the same NodePlayer at (0,0)
    // with the same scale factor — not a far offset in UI space.
    const int32_t spineId   = m_armamentSpineId;
    const std::string anim  = m_spineAnim;
    const int zOrder        = artifactLocalZOrder(cls);
    auto weakThis           = weak_from_this();

    mugen::MgSkeletonAnimation::createFromCacheAsync(
        spineId, [weakThis, this, spineId, anim, zOrder](mugen::MgSkeletonAnimation* skel) {
            if (!skel)
            {
                AXLOGW("WeaponFormPreviewDialog: artifact spine load failed id={}", spineId);
                return;
            }
            if (weakThis.expired() || !m_avatar || m_armamentSpineId != spineId)
                return;

            skel->setAnimation(0, anim, true);
            skel->setPosition(ax::Vec2::ZERO);
            skel->setScale(1.0f);
            m_avatar->addChild(skel, zOrder);
            m_artifactSpine = skel;
            AXLOGI("WeaponFormPreviewDialog: artifact spineId={} anim={} z={} (child of Avatar @ origin)",
                   spineId, anim, zOrder);
        });

    AXLOGI("WeaponFormPreviewDialog: role centered; artifact pending spineId={}", m_armamentSpineId);
}

void WeaponFormPreviewDialog::onClickClose(EventContext*)
{
    close();
}

}  // namespace gameui
