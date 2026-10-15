#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec3.h"
#include <vector>

namespace mugen::sim
{
/// 动作与效果、Buff 模块交互所需的战斗世界。
class World;
namespace effect
{
/// 从角色动作生成效果所需的上下文。
struct RoleSpawn
{
    /// 要生成的效果配置 id。
    int32_t m_effectId = -1;
    /// 发起生成的技能配置 id。
    int32_t m_skillId = -1;
    /// 当前连段首段的技能配置 id。
    int32_t m_baseSkillId = -1;
    /// 发起效果的技能槽位。
    uint16_t m_slot = 0;
    /// 是否使用自定义生成向量。
    bool m_customVector = false;
    /// 自定义生成向量。
    FixedVec3 m_vector;
};
}
namespace attack
{
// 动作与效果、Buff 之间的临时接缝视图，不存入组件、不拥有资源，生命周期限于单次调用。
class AttackSeam
{
public:
    // 构造指向指定世界实例中某个实体的视图。
    AttackSeam(World& world, Entity entity);
    // 使用动作上下文生成角色效果。
    Entity spawnFromRole(const effect::RoleSpawn& request);
    // 清理由动作拥有的一组效果实体。
    void clearActionEffects(const std::vector<Entity>& effects);
    // 为动作期间添加的 Buff 建立来源关系。
    void addActionBuff(int32_t buffId, int32_t baseSkillId);
    // 移除动作结束后不再生效的 Buff。
    void removeActionBuff(int32_t buffId);

private:
    // 本场战斗世界。
    World& m_world;
    // 视图绑定的实体。
    Entity m_entity{};
};
}
}
