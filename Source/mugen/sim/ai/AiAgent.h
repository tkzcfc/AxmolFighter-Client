#pragma once
#include "DecisionTree.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec3.h"

namespace mugen::sim
{
class World;
// 按首次受击顺序保存一个攻击者的仇恨。
struct AiThreat
{
    // 攻击者句柄与累计伤害。
    Entity m_source;
    Fixed m_damage;
    MG_REFLECT(AiThreat, m_source, m_damage)
};
// 每个怪物独占的决策状态；目标和施法间隔由 SkillPool 持有。
struct AiAgent
{
    // 可恢复的决策调用栈。
    ai::DecisionState m_tree;
    // 换目标累计毫秒。
    Fixed m_changeTargetMs;
    // 稳定顺序的仇恨记录。
    std::vector<AiThreat> m_threat;
    // 引导可暂停本角色的 AI，不影响动作树。
    bool m_enabled = true;
    MG_REFLECT(AiAgent, m_tree, m_changeTargetMs, m_threat, m_enabled)
};
namespace ai
{
// 在角色 AI 更新阶段执行决策、嘲讽和十秒仇恨切换。
void update(World& world, Entity entity);
// 分派生产怪物方法，供决策树和动作叶子共用。
Result invoke(World& world, Entity entity, Method method);
// 每帧巡逻索敌；使用带符号的视野区间。
Result checkTarget(World& world, Entity entity);
// 记录命中伤害，不为非 AI 角色创建状态。
void addThreat(World& world, Entity entity, Entity source, Fixed damage);
// 选择严格高于当前目标权重的有效攻击者，然后清空本周期记录。
void changeTarget(World& world, Entity entity);
// 按地图四侧边界和角色半径修正目标点。
FixedVec3 calcPos(World& world, Entity entity, FixedVec3 point);
// 以固定创建顺序收集存活、启用且可命中的敌对阵营组角色。
std::vector<Entity> candidates(World& world, Entity entity);
}
}
