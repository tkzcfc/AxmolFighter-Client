#pragma once

#include "mugen/core/bt/BTNode.h"

NS_MG_BEGIN

class Entity;
class BehaviorTreeComponent;

/**
 * 角色行为树构建（ROLE 优先级）。
 *
 * 树形结构：
 *   RoleRoot(Selector)
 *   ├─ Death(Sequence) / 其余单叶枝(Parallel)
 *   ├─ Attack(Selector) ← SkillTreeBuilder 灌入
 *   │   └─ Slot → SlotIndex → Mode → Step → Pipe → Toward → AttackAction×N
 *   ├─ Jostled / Patrol / Chase / Alert / PathFinding
 *   ├─ Dash / Walk / Idle
 *
 * 拓扑由 Builder 按卡组建；运行时标量（粘滞下标、elapsedMs 等）写在节点上，快照 DFS 填回。
 */
namespace RoleTreeBuilder
{

/** 构建角色非攻击枝 + 空 Attack Selector（技能由 SkillTreeBuilder::fill 灌入） */
BTNode* build(BehaviorTreeComponent* bt);

/** 城镇精简树：仅 Walk / Idle（无 Attack/Hit） */
BTNode* buildCity(BehaviorTreeComponent* bt);

/** 给实体挂树（要求已有 BehaviorTreeComponent）；反序列化后 root 为空时也会调用 */
void attachToEntity(Entity* entity);

/** 快照恢复后按 CondRoleAttack 回绑 attackSelector */
void rebindAttackSelector(BehaviorTreeComponent* bt);

}  // namespace RoleTreeBuilder

NS_MG_END
