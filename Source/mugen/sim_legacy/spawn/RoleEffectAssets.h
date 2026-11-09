#pragma once

#include <cstdint>
#include <vector>

namespace mugen::sim_legacy
{

// 收集角色在战斗中会创建的特效 Spine 资源 id（res_spine）。
// 技能装配与 RoleSpawner::spawnRole 相同：职业角色用 offlineKit，其余用 AI loadout。
// aiDifficulty 与 RoleSpawnParams 一致：>0 时按 1-based 下标取 role.aiId，否则取最后一项。
// 结果按 id 升序去重；无效或缺失的配置不输出。
std::vector<int32_t> collectRoleEffectSpineIds(int32_t roleId, int32_t aiDifficulty = 0);

}  // namespace mugen::sim_legacy
