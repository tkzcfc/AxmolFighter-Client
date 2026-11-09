#include "resource/builtin/RoleEffectHelper.h"

#include "resource/ResourceLoader.h"
#include "resource/builtin/SpineResource.h"

#include "mugen/sim_legacy/spawn/RoleEffectAssets.h"

namespace gameres
{

void addRoleEffects(ResourceLoader& loader, int32_t roleId, int32_t aiDifficulty)
{
    for (const int32_t spineId : mugen::sim_legacy::collectRoleEffectSpineIds(roleId, aiDifficulty))
        loader.add<SpineResource>(spineId);
}

}  // namespace gameres
