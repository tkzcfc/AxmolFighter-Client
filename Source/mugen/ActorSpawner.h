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

// 角色身体 Spine 的解析结果：骨架路径、atlas 路径、加载缩放。
// skeleton 为空表示该角色没有配置 Spine（valid=false）。
struct RoleSpineInfo
{
    bool valid = false;
    std::string skeleton;
    std::string atlas;
    float scale = 1.0f;
};

// 按角色 id 算出身体 Spine 的骨架/atlas/缩放，preferCity 为 true 时优先用城镇 *_city 覆盖
// （如果文件存在）。预加载和 spawnRoleActor 共用这一套规则，确保 SpineSkeletonCache 的
// 缓存 key 完全一致（见 gameres::addRoleSpine）。
RoleSpineInfo resolveRoleSpine(int32_t roleId, bool preferCity);

Entity* spawnRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params);

Entity* spawnRemoteRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params);

}  // namespace actor_spawner

NS_MG_END
