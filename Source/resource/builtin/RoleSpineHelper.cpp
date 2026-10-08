#include "resource/builtin/RoleSpineHelper.h"

#include "resource/ResourceLoader.h"
#include "resource/builtin/SpineResource.h"

#include "mugen/avatar/AvatarPaths.h"

namespace gameres
{

std::shared_ptr<SpineResource> addRoleSpine(ResourceLoader& loader, int32_t roleId)
{
    const auto info = mugen::AvatarPaths::resolveRoleSpine(roleId);
    if (!info.valid)
        return nullptr;

    return loader.add<SpineResource>(info.skeleton, std::vector<std::string>{info.atlas}, info.scale);
}

}  // namespace gameres
