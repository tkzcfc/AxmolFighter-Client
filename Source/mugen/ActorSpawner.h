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
};

// 兼容旧调用点
struct PlayerSpawnParams
{
    int32_t playerId = 0;
    std::string name;

    ActorSpawnParams toActorParams(EntityCategory category = EntityCategory::kPlayer) const
    {
        ActorSpawnParams p;
        p.category = category;
        p.playerId = playerId;
        p.name     = name;
        return p;
    }
};

// 将服务器 class_id（JobType：1/2/4）解析为可玩英雄 RoleConfig id（101/102/103）。
// 若传入已是有效英雄 RoleConfig id（带 /hero/ spine），原样返回；否则回退 101。
int32_t resolvePlayableRoleId(int32_t classOrRoleId);

Entity* spawnRolePlayerActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y);
Entity* spawnRolePlayerActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params);
Entity* spawnRolePlayerActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const PlayerSpawnParams& params);

Entity* spawnRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params);

Entity* spawnRemoteRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params);

}  // namespace actor_spawner

NS_MG_END
