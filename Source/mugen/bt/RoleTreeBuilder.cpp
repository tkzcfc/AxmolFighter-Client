#include "mugen/bt/RoleTreeBuilder.h"

#include "mugen/bt/EffectTreeBuilder.h"
#include "mugen/bt/SkillTreeBuilder.h"
#include "mugen/bt/actions/AIActions.h"
#include "mugen/bt/actions/RoleActions.h"
#include "mugen/bt/conditions/CondAI.h"
#include "mugen/bt/conditions/CondAttack.h"
#include "mugen/bt/conditions/CondStatus.h"
#include "mugen/component/BehaviorTreeComponent.h"
#include "mugen/core/bt/BTComposite.h"
#include "mugen/core/bt/BTParallel.h"
#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTSequence.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/Components.h"

#include <cstring>

NS_MG_BEGIN

namespace RoleTreeBuilder
{

namespace
{

BTSequence* makeDeathBranch()
{
    auto* seq = new BTSequence();
    seq->addCondition(new CondStatus(BehaviorKind::kDeath));
    seq->addChild(new DeathAction());
    return seq;
}

BTParallel* makeBranch(BehaviorKind kind, BTAction* action)
{
    auto* par = new BTParallel();
    par->addCondition(new CondStatus(kind));
    par->addChild(action);
    return par;
}

BTParallel* makeCondBranch(BTCondition* cond, BTAction* action)
{
    auto* par = new BTParallel();
    par->addCondition(cond);
    par->addChild(action);
    return par;
}

}  // namespace

void rebindAttackSelector(BehaviorTreeComponent* bt)
{
    bt->attackSelector = nullptr;
    auto* tree         = bt->ensureTree();
    if (!tree->getRoot())
        return;
    auto* rootComp = static_cast<BTComposite*>(tree->getRoot());
    for (BTNode* child : rootComp->getChildren())
    {
        auto* branch = dynamic_cast<BTComposite*>(child);
        if (!branch)
            continue;
        for (BTCondition* cond : branch->getConditions())
        {
            if (std::strcmp(cond->typeName(), "CondRoleAttack") != 0)
                continue;
            bt->attackSelector = child;
            return;
        }
    }
}

BTNode* build(BehaviorTreeComponent* bt)
{
    if (!bt)
        return nullptr;

    bt->attackSelector = nullptr;
    bt->treeKind       = 0;

    auto* root = new BTSelector();

    root->addChild(makeDeathBranch());
    root->addChild(makeBranch(BehaviorKind::kRevive, new ReviveAction()));
    root->addChild(makeBranch(BehaviorKind::kWake, new WakeAction()));
    root->addChild(makeBranch(BehaviorKind::kGetUp, new HitKindAction(BehaviorKind::kGetUp)));
    root->addChild(makeBranch(BehaviorKind::kHitFloor, new HitKindAction(BehaviorKind::kHitFloor)));
    root->addChild(makeBranch(BehaviorKind::kHitDown, new HitKindAction(BehaviorKind::kHitDown)));
    root->addChild(makeBranch(BehaviorKind::kHitUp, new HitKindAction(BehaviorKind::kHitUp)));
    root->addChild(makeBranch(BehaviorKind::kHitSwitch, new HitKindAction(BehaviorKind::kHitSwitch)));
    root->addChild(makeBranch(BehaviorKind::kStun, new HitKindAction(BehaviorKind::kStun)));

    auto* attackSel = new BTSelector();
    attackSel->addCondition(new CondRoleAttack());
    bt->attackSelector = attackSel;
    root->addChild(attackSel);

    root->addChild(makeCondBranch(new CondJostled(), new JostledAction()));
    root->addChild(makeCondBranch(new CondPatrol(), new PatrolAction()));
    root->addChild(makeCondBranch(new CondChase(), new ChaseAction()));
    root->addChild(makeCondBranch(new CondAlert(), new AlertAction()));
    root->addChild(makeCondBranch(new CondPathFinding(), new PathFindingAction()));

    root->addChild(makeBranch(BehaviorKind::kDash, new LocomoAction(BehaviorKind::kDash)));
    root->addChild(makeBranch(BehaviorKind::kWalk, new LocomoAction(BehaviorKind::kWalk)));
    root->addChild(makeBranch(BehaviorKind::kIdle, new LocomoAction(BehaviorKind::kIdle)));

    return root;
}

BTNode* buildCity(BehaviorTreeComponent* bt)
{
    if (!bt)
        return nullptr;

    bt->attackSelector = nullptr;
    bt->treeKind       = 1;

    auto* root = new BTSelector();

    root->addChild(makeBranch(BehaviorKind::kDash, new LocomoAction(BehaviorKind::kDash)));
    root->addChild(makeBranch(BehaviorKind::kWalk, new LocomoAction(BehaviorKind::kWalk)));
    root->addChild(makeBranch(BehaviorKind::kIdle, new LocomoAction(BehaviorKind::kIdle)));

    return root;
}

void attachToEntity(Entity* entity)
{
    if (!entity)
        return;
    auto* bt = BehaviorTreeComponent::of(entity);
    if (!bt)
        return;
    if (bt->treeKind == 2)
    {
        EffectTreeBuilder::attachToEntity(entity);
        return;
    }
    auto* tree = bt->ensureTree();
    tree->setRoot(bt->cityMode ? buildCity(bt) : build(bt));
    if (tree->getRoot() && !bt->cityMode)
        SkillTreeBuilder::fill(entity);
    rebindAttackSelector(bt);
}

}  // namespace RoleTreeBuilder

NS_MG_END
