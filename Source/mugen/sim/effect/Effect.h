#pragma once
#include "mugen/core/bt/BTRunner.h"
#include "mugen/sim/attack/AttackSeam.h"
#include "mugen/sim/attack/AttackState.h"
#include "mugen/sim/motion/Displacement.h"

namespace mugen::sim
{
/// 特效视图所包装的战斗世界实例。
class World;
/// 特效动作的值类型叶状态。
struct AttackEffect
{
    /// 共用动作时序及子特效。
    attack::ActionAttack m_action;
    /// 本段是否已提交召唤请求。
    bool m_summoned = false;
    MG_REFLECT(AttackEffect, m_action, m_summoned)
};
/// 特效实体的归属、生命周期和命中流程共享数据。
struct EffectState
{
    /// 特效配置标识。
    int32_t m_configId = -1;
    /// 属性唯一来源，不复制角色属性。
    Entity m_owner;
    /// 用于选择受击音效的目标。
    Entity m_hitEntity;
    /// 当前技能及槽首技能。
    int32_t m_skillId = -1, m_baseSkillId = -1;
    /// 技能槽位及属性来源标识。
    uint16_t m_slot = 0;
    Entity m_skillAttributeTag;
    /// 当前冻结剩余时间。
    Fixed m_freezeMs;
    /// 上一帧结算的冻结状态。
    bool m_frozen = false;
    /// 是否允许结束后派生后继特效。
    bool m_next = true;
    /// 本帧命中次数及间隔。
    int32_t m_hitCount = 0;
    Fixed m_hitInterval;
    /// 单次命中模式已经处理的目标。
    std::vector<Entity> m_hitTargets;
    // 神器来源及索引，负一表示无来源。
    int32_t m_artifactId = -1, m_artifactIndex = -1;
    // 能力触发来源不递归触发命中能力。
    bool m_capabilityTrigger = false;
    MG_REFLECT(EffectState,
               m_configId,
               m_owner,
               m_hitEntity,
               m_skillId,
               m_baseSkillId,
               m_slot,
               m_skillAttributeTag,
               m_freezeMs,
               m_frozen,
               m_next,
               m_hitCount,
               m_hitInterval,
               m_hitTargets,
               m_artifactId,
               m_artifactIndex,
               m_capabilityTrigger)
};
/// 特效树只存配置键与可快照实例。
struct EffectTree
{
    /// 定义键为按顺序排列的动作标识。
    std::vector<int32_t> m_actions;
    /// 节点游标和叶运行状态。
    BTInstance<std::variant<AttackEffect>> m_instance;
    MG_REFLECT(EffectTree, m_actions, m_instance)
};
namespace effect
{
// 特效实体的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class EffectActor
{
public:
    // 工厂：从主人实体新建特效，而非对既有特效视图的操作。
    static Entity create(World& world, Entity owner, int32_t id);
    // 实体组回调，须保持 void(World&, Entity) 签名以注册到 WorldHooks。
    static void update(World& world, Entity entity);
    static void remove(World& world, Entity entity);
    static void removeOwner(World& world, Entity owner);
    // 构造指向指定世界实例中某个特效实体的视图。
    EffectActor(World& world, Entity entity);
    void setCustomPosition(Entity base, bool ground = false);
    void follow();
    Entity spawnChild(int32_t id);
    void summon(int32_t roleId, int64_t lifetimeMs);
    void enter();
    bool onDisplacementEvent(DisplacementEvent event);
    void animationComplete(int64_t loops);

private:
    World& m_world;
    Entity m_entity{};
};
}  // namespace effect
}  // namespace mugen::sim
