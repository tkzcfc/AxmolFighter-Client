#pragma once
#include "mugen/core/bt/BTRunner.h"
#include "mugen/sim/attack/AttackSeams.h"
#include "mugen/sim/attack/AttackState.h"
#include "mugen/sim/motion/Displacement.h"

namespace mugen::sim
{
namespace spawn
{
/// 在效果位置生成继承主人阵营和属性的限时角色。
void summon(World&, Entity effect, int32_t roleId, int64_t lifetimeMs);
}
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
    MG_REFLECT(EffectState, m_configId, m_owner, m_hitEntity, m_skillId, m_baseSkillId,
               m_slot, m_skillAttributeTag, m_freezeMs, m_frozen, m_next, m_hitCount,
               m_hitInterval, m_hitTargets, m_artifactId, m_artifactIndex, m_capabilityTrigger)
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
/// 创建尚未进入动作的特效实体。
Entity create(World&, Entity owner, int32_t id);
/// 设置相对于基准实体的位置及朝向。
void setCustomPosition(World&, Entity effect, Entity base, bool ground = false);
/// 按配置更新跟随坐标。
void follow(World&, Entity);
/// 从父特效继承归属和技能上下文并进入子特效。
Entity spawnChild(World&, Entity parent, int32_t id);
/// 启动效果行为树。
void enter(World&, Entity);
/// 推进效果组中的实体。
void update(World&, Entity);
/// 退出动作并回收动作拥有的子效果。
void remove(World&, Entity);
/// 角色移除时清理与角色绑定的特效。
void removeOwner(World&, Entity);
/// 接收运动系统的位移通知。
bool onDisplacementEvent(World&, Entity, DisplacementEvent);
/// 将动画结束通知写入正在运行的叶子。
void animationComplete(World&, Entity, int64_t loops);
}
}
