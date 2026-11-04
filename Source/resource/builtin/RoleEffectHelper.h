#pragma once

#include <cstdint>

namespace gameres
{

class ResourceLoader;

// 按角色 id 预热其战斗技能会创建的特效 Spine。
// 技能装配与 RoleSpawner::spawnRole 相同；aiDifficulty 仅对非职业怪物生效。
// 每个 res_spine id 以 SpineResource(resSpineId) 加入 loader，和运行时
// MgSkeletonAnimation::createFromCache(resSpineId) 共用缓存 key。
void addRoleEffects(ResourceLoader& loader, int32_t roleId, int32_t aiDifficulty = 0);

}  // namespace gameres
