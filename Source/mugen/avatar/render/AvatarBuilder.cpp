#include "AvatarBuilder.h"

#ifdef RUNTIME_IN_AXMOL

#    include "Avatar.h"
#    include "SpineLayer.h"
#    include "mugen/component/AvatarComponent.h"

NS_MG_BEGIN

Avatar* AvatarBuilder::createAvatar(const FashionSpineDesc& desc, bool asyncLoad)
{
    if (desc.skeleton.empty() || desc.atlases.empty())
    {
        MG_LOG_E("AvatarBuilder: FashionSpineDesc invalid");
        return nullptr;
    }

    Avatar* avatar = Avatar::create();
    if (!avatar)
    {
        MG_LOG_E("AvatarBuilder: Avatar::create failed");
        return nullptr;
    }

    SpineLayer* layer = SpineLayer::create(desc, asyncLoad);
    if (!layer)
    {
        MG_LOG_E("AvatarBuilder: SpineLayer::create failed '{}'", desc.skeleton);
        return nullptr;
    }
    avatar->addLayer(layer, 0, AvatarLayerTag::kBody);
    return avatar;
}

Avatar* AvatarBuilder::createAvatar(const AvatarComponent* avatarComp)
{
    if (!avatarComp)
    {
        MG_LOG_E("AvatarBuilder: AvatarComponent is null");
        return nullptr;
    }

    FashionSpineDesc desc;
    desc.skeleton            = avatarComp->getSpineSkeleton();
    desc.skin                = avatarComp->defaultSkin;
    desc.scale               = avatarComp->getSpineScale();
    desc.motionFile          = avatarComp->motionFile;
    const std::string& atlas = avatarComp->getSpineAtlas();
    if (!atlas.empty())
        desc.atlases.push_back(atlas);
    return createAvatar(desc, false);
}

Avatar* AvatarBuilder::createAvatar(const FashionAppearance& appearance, bool asyncLoad)
{
    const auto desc = FashionResolver::resolve(appearance);
    if (!desc.valid)
    {
        MG_LOG_E("AvatarBuilder: resolve fashion failed role={}", appearance.roleId);
        return nullptr;
    }

    Avatar* avatar = Avatar::create();
    if (!avatar)
    {
        MG_LOG_E("AvatarBuilder: Avatar::create failed");
        return nullptr;
    }
    avatar->initFashionAppearance(appearance);

    SpineLayer* layer = SpineLayer::create(desc, asyncLoad);
    if (!layer)
    {
        MG_LOG_E("AvatarBuilder: SpineLayer::create failed '{}'", desc.skeleton);
        return nullptr;
    }
    avatar->addLayer(layer, 0, AvatarLayerTag::kBody);
    return avatar;
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
