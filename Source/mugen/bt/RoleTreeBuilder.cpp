#include "mugen/bt/RoleTreeBuilder.h"

#include "mugen/bt/SkillTreeBuilder.h"
#include "mugen/bt/actions/RoleActions.h"
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

    // Death > GetUp > HitFloor > HitUp > Stun > Attack > Dash > Walk > Idle
    root->addChild(makeBranch(btComp, BehaviorKind::kDeath, std::make_unique<DeathAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kGetUp, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kHitFloor, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kHitUp, std::make_unique<HitReactAction>()));
    root->addChild(makeBranch(btComp, BehaviorKind::kStun, std::make_unique<HitReactAction>()));

    auto attackSel           = std::make_unique<BTSelector>();
    attackSel->memorySlot    = btComp->allocMemorySlot();
    attackSel->debugName     = "Attack";
    attackSel->addCondition(std::make_unique<CondRoleAttack>());
    // 子树由 SkillTreeBuilder::fill 灌入
    btComp->attackSelector = attackSel.get();
    root->addChild(std::move(attackSel));

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
    bt->root = build(bt);
    if (bt->root)
    {
        const int32_t slots = (std::max)(bt->nextMemorySlot, 1);
        bt->selectorMemory.assign(static_cast<size_t>(slots), static_cast<int8_t>(-1));
        SkillTreeBuilder::fill(entity);
    }
}

}  // namespace RoleTreeBuilder

NS_MG_END
