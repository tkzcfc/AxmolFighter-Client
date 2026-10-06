#include "resource/builtin/RoleSpineHelper.h"

#include "resource/ResourceLoader.h"
#include "resource/builtin/SpineResource.h"

#include "mugen/ActorSpawner.h"

namespace gameres
{

std::shared_ptr<SpineResource> addRoleSpine(ResourceLoader& loader, int32_t roleId, bool preferCity)
{
    const auto info = mugen::actor_spawner::resolveRoleSpine(roleId, preferCity);
    if (!info.valid)
        return nullptr;

    return loader.add<SpineResource>(info.skeleton, std::vector<std::string>{info.atlas}, info.scale);
}

}  // namespace gameres
