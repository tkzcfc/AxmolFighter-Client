#pragma once
#include "mugen/sim/World.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/battle/RoleDef.h"
#include <array>
#include <optional>

namespace mugen::sim
{
// 创建角色时使用的身份、阵营、位置和控制归属。
struct RoleSpawnParams
{
    // 角色配置标识。
    int32_t m_roleId = 0;
    // 可选覆盖配置默认阵营。
    std::optional<int32_t> m_camp;
    // 角色等级。
    int32_t m_level = 1;
    // AI 难度等级。
    int32_t m_aiDifficulty = 1;
    // 出生坐标。
    FixedVec3 m_position;
    // 初始朝向，仅取 -1 或 1。
    int32_t m_toward = 1;
    // 可选绑定到本场世界的玩家槽位。
    std::optional<uint32_t> m_playerSlot;
    // 是否由 AI 驱动。
    bool m_isAi = false;
    // 召唤归属。
    Entity m_owner;
};
namespace role::rules
{
// 按角色类型、等级和属性倍率计算初始基础属性（移植旧实现 RoleSpawner::loadAttributes）。
std::array<Fixed, kBaseStatCount> loadAttributes(const conf::battle::RoleDef& role, int32_t level);
// 将阵营位掩码转换为可碰撞的敌对阵营掩码（移植旧实现 RoleSpawner::_hostile）。
int32_t hostileMask(int32_t camp);
// 按配置生成角色：追加一条 SpawnRequest 到队列，由 SpawnFlush 实体化。
void spawnRole(World& world, const RoleSpawnParams& params);
// 由职业生成玩家角色（复用 spawnRole）。
void spawnPlayer(World& world, CharacterClass occupation, RoleSpawnParams params);
}  // namespace role::rules
}  // namespace mugen::sim
