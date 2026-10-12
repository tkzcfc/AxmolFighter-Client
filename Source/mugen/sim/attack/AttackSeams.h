#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec3.h"

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
/// 效果模块负责效果归属、跟随规则和自动释放筛选。
/// 使用动作上下文生成角色效果。
Entity spawnFromRole(World&, Entity, const RoleSpawn&);
/// 清理由动作拥有的一组效果实体。
void clearActionEffects(World&, Entity, const std::vector<Entity>&);
}
namespace buff
{
/// 为动作期间添加的 Buff 建立来源关系。
void addActionBuff(World&, Entity, int32_t buffId, int32_t baseSkillId);
/// 移除动作结束后不再生效的 Buff。
void removeActionBuff(World&, Entity, int32_t buffId);
}
}
