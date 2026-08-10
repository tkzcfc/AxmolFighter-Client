#include "mugen/bt/RoleTreeBuilder.h"

#include "mugen/bt/SkillTreeBuilder.h"
#include "mugen/bt/actions/AIActions.h"
#include "mugen/bt/actions/RoleActions.h"
#include "mugen/bt/conditions/CondAI.h"
#include "mugen/bt/conditions/CondAttack.h"
#include "mugen/bt/conditions/CondStatus.h"
#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTSequence.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/Components.h"

#include <algorithm>

NS_MG_BEGIN

namespace RoleTreeBuilder
{

namespace
{

std::unique_ptr<BTSelector> makeBranch(BehaviorTreeComponent* bt,
                                       BehaviorKind kind,
                                       std::unique_ptr<BTAction> action)
{
    auto sel = std::make_unique<BTSelector>();
    sel->memorySlot = bt->allocMemorySlot();
    sel->debugName  = "Branch";
    sel->addCondition(std::make_unique<CondStatus>(kind));
    sel->addChild(std::move(action));
    return sel;
}

std::unique_ptr<BTSelector> makeCondBranch(BehaviorTreeComponent* bt,
                                           std::unique_ptr<BTCondition> cond,
                                           std::unique_ptr<BTAction> action,
                                           const char* name)
{
    auto sel = std::make_unique<BTSelector>();
    sel->memorySlot = bt->allocMemorySlot();
    sel->debugName  = name ? name : "AIBranch";
    sel->addCondition(std::move(cond));
    sel->addChild(std::move(action));
    return sel;
}

}  // namespace

std::unique_ptr<BTNode> build(BehaviorTreeComponent* btComp)
{
    if (!btComp)
        return nullptr;

    btComp->nextMemorySlot = 0;
    btComp->selectorMemory.clear();
    btComp->attackSelector = nullptr;

    auto root           = std::make_unique<BTSelector>();
    root->memorySlot    = btComp->allocMemorySlot();
    root->debugName     = "RoleRoot";

    // Death > GetUp > Hit* > Attack > Jostled > Alert > Chase > Patrol > Dash > Walk > Idle
    root->addChild(makeBranch(btComp, BehaviorKind::kDeath, std::make_unique<DeathAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kGetUp, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kHitFloor, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kHitDown, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kHitUp, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kHitSwitch, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kStun, std::make_unique<HitReactAction>()));

    auto attackSel           = std::make_unique<BTSelector>();
    attackSel->memorySlot    = btComp->allocMemorySlot();
    attackSel->debugName     = "Attack";
    attackSel->addCondition(std::make_unique<CondRoleAttack>());
    btComp->attackSelector = attackSel.get();
    root->addChild(std::move(attackSel));

    root->addChild(makeCondBranch(btComp, std::make_unique<CondJostled>(), std::make_unique<JostledAction>(), "Jostled"));
    root->addChild(makeCondBranch(btComp, std::make_unique<CondAlert>(), std::make_unique<AlertAction>(), "Alert"));
    root->addChild(makeCondBranch(btComp, std::make_unique<CondChase>(), std::make_unique<ChaseAction>(), "Chase"));
    root->addChild(makeCondBranch(btComp, std::make_unique<CondPatrol>(), std::make_unique<PatrolAction>(), "Patrol"));

    root->addChild(makeBranch(btComp, BehaviorKind::kDash, std::make_unique<LocomoAction>(BehaviorKind::kDash)));
    root->addChild(makeBranch(btComp, BehaviorKind::kWalk, std::make_unique<LocomoAction>(BehaviorKind::kWalk)));
    root->addChild(makeBranch(btComp, BehaviorKind::kIdle, std::make_unique<LocomoAction>(BehaviorKind::kIdle)));

    return root;
}

std::unique_ptr<BTNode> buildCity(BehaviorTreeComponent* btComp)
{
    if (!btComp)
        return nullptr;

    btComp->nextMemorySlot = 0;
    btComp->selectorMemory.clear();
    btComp->attackSelector = nullptr;

    auto root        = std::make_unique<BTSelector>();
    root->memorySlot = btComp->allocMemorySlot();
    root->debugName  = "CityRoleRoot";

    root->addChild(makeBranch(btComp, BehaviorKind::kDash, std::make_unique<LocomoAction>(BehaviorKind::kDash)));
    root->addChild(makeBranch(btComp, BehaviorKind::kWalk, std::make_unique<LocomoAction>(BehaviorKind::kWalk)));
    root->addChild(makeBranch(btComp, BehaviorKind::kIdle, std::make_unique<LocomoAction>(BehaviorKind::kIdle)));

    return root;
}

void attachToEntity(Entity* entity)
{
    if (!entity)
        return;
    auto* bt = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
    if (!bt)
        return;
    bt->root = bt->cityMode ? buildCity(bt) : build(bt);
    if (bt->root)
    {
        const int32_t slots = (std::max)(bt->nextMemorySlot, 1);
        bt->selectorMemory.assign(static_cast<size_t>(slots), static_cast<int8_t>(-1));
        if (!bt->cityMode)
            SkillTreeBuilder::fill(entity);
    }
}

}  // namespace RoleTreeBuilder

NS_MG_END
