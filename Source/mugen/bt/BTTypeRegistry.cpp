#include "mugen/bt/BTTypeRegistry.h"

#include "mugen/core/bt/BTFactory.h"
#include "mugen/core/bt/BTParallel.h"
#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTSequence.h"

#include "mugen/bt/actions/AIActions.h"
#include "mugen/bt/actions/AttackAction.h"
#include "mugen/bt/actions/RoleActions.h"
#include "mugen/bt/conditions/CondAI.h"
#include "mugen/bt/conditions/CondAttack.h"
#include "mugen/bt/conditions/CondEffect.h"
#include "mugen/bt/conditions/CondStatus.h"

NS_MG_BEGIN

namespace BTTypeRegistry
{

void registerAll()
{
#define REGISTER_NODE(name)      factory->registerNode(#name, []() { return new name(); })
#define REGISTER_CONDITION(name) factory->registerCondition(#name, []() { return new name(); })
    auto* factory = BTFactory::getInstance();

    // 只执行一次注册逻辑
    if (factory->hasNode("BTParallel"))
    {
        return;
    }

    // 基础节点
    REGISTER_NODE(BTParallel);
    REGISTER_NODE(BTSelector);
    REGISTER_NODE(BTSequence);
    
    // 动作节点
    REGISTER_NODE(AttackAction);
    REGISTER_NODE(LocomoAction);
    REGISTER_NODE(HitKindAction);
    REGISTER_NODE(TimedKindAction);
    REGISTER_NODE(DeathAction);
    REGISTER_NODE(WakeAction);
    REGISTER_NODE(ReviveAction);
    REGISTER_NODE(HoldAttackAction);
    REGISTER_NODE(PatrolAction);
    REGISTER_NODE(AlertAction);
    REGISTER_NODE(ChaseAction);
    REGISTER_NODE(JostledAction);
    REGISTER_NODE(PathFindingAction);
    REGISTER_NODE(EffectHoldAction);

    // 条件节点
    REGISTER_CONDITION(CondStatus);
    REGISTER_CONDITION(CondRoleAttack);
    REGISTER_CONDITION(CondAttackSlot);
    REGISTER_CONDITION(CondAttackStep);
    REGISTER_CONDITION(CondAttackPipe);
    REGISTER_CONDITION(CondAttackSlotIndex);
    REGISTER_CONDITION(CondAttackMode);
    REGISTER_CONDITION(CondAttackToward);
    REGISTER_CONDITION(CondPatrol);
    REGISTER_CONDITION(CondAlert);
    REGISTER_CONDITION(CondChase);
    REGISTER_CONDITION(CondJostled);
    REGISTER_CONDITION(CondPathFinding);
    REGISTER_CONDITION(CondEffectAlive);
    REGISTER_CONDITION(CondEffectAttack);

#undef REGISTER_NODE
#undef REGISTER_CONDITION
}

}  // namespace BTTypeRegistry

NS_MG_END
