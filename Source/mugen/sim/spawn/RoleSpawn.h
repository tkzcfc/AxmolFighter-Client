#pragma once
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/sim/buff/BuffPool.h"
#include <array>

namespace mugen::sim
{
// 描述创建角色时使用的身份、阵营、位置和控制归属。
struct RoleSpawnParams
{
    // 指向角色配置行的唯一标识。
    int32_t roleId = 0;
    // 可选覆盖配置默认阵营。
    std::optional<int32_t> camp;
    // 决定属性曲线与成长数据的角色等级。
    int32_t level = 1;
    // 选择角色 AI 配置数组中的难度索引。
    int32_t aiDifficulty = 1;
    // 指定生成时的三维逻辑坐标。
    FixedVec3 position;
    // 限定初始朝向只能为 -1 或 1。
    int32_t toward = 1;
    // 可选绑定到本场世界的玩家槽位。
    std::optional<uint32_t> playerSlot;
    // 标识角色是否由 AI 自动驱动。
    bool isAi = false;
    // 召唤归属在属性和出生效果装配前确定。
    Entity m_owner;
    // 成长、芯片和额外效果装配，调用方保留既定来源顺序。
    std::vector<buff::LoadEntry> m_buffs;
};
// 按配置创建战斗角色的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class RoleSpawner
{
public:
    // 构造指向指定世界实例的出生视图。
    explicit RoleSpawner(World& world);
    // 按角色配置和等级计算初始战斗属性。
    static Attributes loadAttributes(const conf::RoleConfig& role, int32_t level);
    // 创建拥有完整战斗组件、技能和行为树的角色实体。
    Entity spawnRole(const RoleSpawnParams& params);
    // 根据职业转换后的配置标识创建玩家角色。
    Entity spawnPlayer(CharacterClass occupation, RoleSpawnParams params);
    // 创建仅由外部同步驱动的城镇木偶实体。
    Entity spawnPuppet(int64_t playerId, int32_t roleId, FixedVec3 position, int32_t toward);

private:
    // 将配置中的基础属性按运行时属性槽位顺序转换为 Fixed 数组。
    static std::array<Fixed, 16> _values(const conf::AttributeConfig& attribute);
    // 将阵营位掩码转换为可碰撞的敌对阵营掩码。
    static int32_t _hostile(int32_t camp);

    // 本场战斗世界。
    World& m_world;
};
}  // namespace mugen::sim
