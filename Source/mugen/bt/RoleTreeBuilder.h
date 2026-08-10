#pragma once

#include "mugen/core/bt/BTNode.h"
#include "mugen/component/BehaviorTreeComponent.h"

#include <memory>

NS_MG_BEGIN

class Entity;

/**
 * 角色行为树构建（ROLE 优先级）。
 *
 * 树形结构：
 *   RoleRoot(Selector sticky)
 *   ├─ Death / GetUp / HitFloor / HitDown / HitUp / HitSwitch / Stun
 *   ├─ Attack(Selector) ← SkillTreeBuilder 灌入
 *   │   └─ Slot → Step → Pipe → Toward → AttackAction×N
 *   ├─ Jostled / Alert / Chase / Patrol
 *   ├─ Dash / Walk / Idle
 *
 * 运行时状态全部在组件（SkillCast / BehaviorTree / HitReact / AI），节点可重建。
 */
namespace RoleTreeBuilder
{

/** 构建角色非攻击枝 + 空 Attack Selector（技能由 SkillTreeBuilder::fill 灌入） */
std::unique_ptr<BTNode> build(BehaviorTreeComponent* btComp);

/** 城镇精简树：仅 Walk / Idle（无 Attack/Hit） */
std::unique_ptr<BTNode> buildCity(BehaviorTreeComponent* btComp);

/** 给实体挂树（要求已有 BehaviorTreeComponent）；反序列化后 root 为空时也会调用 */
void attachToEntity(Entity* entity);

}  // namespace RoleTreeBuilder

NS_MG_END
