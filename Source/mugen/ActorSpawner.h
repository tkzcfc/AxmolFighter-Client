#pragma once

#include "mugen/core/ecs/ECSManager.h"
#include "mugen/conf/GameDef.h"

#include <cstdint>
#include <string>

NS_MG_BEGIN

namespace actor_spawner
{

struct ActorSpawnParams
{
    EntityCategory category = EntityCategory::kPlayer;
    int32_t playerId        = 0;
    std::string name;
    bool cityMode = false;  // 城镇精简行为树
    int32_t level = 1;      // 怪物等级（属性模板按 baseIndex[roleType] + level 取）
};

Entity* spawnRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params);

Entity* spawnRemoteRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params);

}  // namespace actor_spawner

NS_MG_END
