#include "AvatarBuilder.h"

#ifdef RUNTIME_IN_AXMOL

#    include "Avatar.h"
#    include "SpineLayer.h"
#    include "mugen/component/AvatarComponent.h"

NS_MG_BEGIN

Avatar* AvatarBuilder::createAvatar(const SpineAvatarDesc& desc)
{
    Avatar* avatar = Avatar::create();
    if (!avatar)
    {
        MG_LOG_E("AvatarBuilder: Avatar::create failed");
        return nullptr;
    }

    SpineLayer* layer = SpineLayer::create(desc);
    if (!layer)
    {
        MG_LOG_E("AvatarBuilder: SpineLayer::create failed");
        return nullptr;
    }
    avatar->addLayer(layer, 0, kAvatarLayerTagCharacter);
    return avatar;
}

Avatar* AvatarBuilder::createAvatar(const AvatarComponent* avatarComp)
{
    if (!avatarComp)
    {
        MG_LOG_E("AvatarBuilder: AvatarComponent is null");
        return nullptr;
    }

    SpineAvatarDesc desc;
    desc.skeleton    = avatarComp->getSpineSkeleton();
    desc.atlas       = avatarComp->getSpineAtlas();
    desc.motionFile  = avatarComp->motionFile;
    desc.defaultSkin = avatarComp->defaultSkin;
    desc.scale       = avatarComp->getSpineScale();
    return createAvatar(desc);
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
