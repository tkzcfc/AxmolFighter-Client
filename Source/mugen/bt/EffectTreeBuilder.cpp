#include "mugen/bt/EffectTreeBuilder.h"

#include "mugen/Components.h"
#include "mugen/bt/actions/AttackAction.h"
#include "mugen/bt/conditions/CondEffect.h"
#include "mugen/component/BehaviorTreeComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTSequence.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/effect/Effect.h"

NS_MG_BEGIN

namespace EffectTreeBuilder
{

BTNode* build(BehaviorTreeComponent* bt, Entity* entity)
{
    if (!bt)
        return nullptr;
    bt->attackSelector = nullptr;
    bt->treeKind       = 2;

    auto* root = new BTSelector();

    auto* fx        = Effect::of(entity);
    const auto* cfg = (fx && fx->effectId > 0) ? Config::getInstance()->getEffectConfigById(fx->effectId) : nullptr;
    if (cfg)
    {
        auto* seq                 = new BTSequence();
        int32_t actionIndex       = 0;
        const int32_t skillLookup = fx ? fx->skillHitId : 0;
        for (int32_t aid : cfg->actionIds)
        {
            if (aid <= 0)
                continue;
            seq->addChild(new AttackAction(aid, actionIndex++, skillLookup, true));
        }
        if (seq->childCount() > 0)
        {
            auto* atk = new BTSelector();
            atk->addCondition(new CondEffectAttack());
            atk->addChild(seq);
            root->addChild(atk);
        }
        else
        {
            delete seq;
        }
    }

    auto* live = new BTSelector();
    live->addCondition(new CondEffectAlive());
    live->addChild(new EffectHoldAction());
    root->addChild(live);
    return root;
}

void attachToEntity(Entity* entity)
{
    if (!entity)
        return;
    auto* bt = BehaviorTreeComponent::of(entity);
    if (!bt)
        return;
    if (auto* fxComp = MG_GET_COMPONENT(entity, EffectComponent))
    {
        fxComp->ensureEffect();
        fxComp->restoreRuntimeData();
    }
    bt->treeKind = 2;
    auto* tree   = bt->ensureTree();
    tree->setRoot(build(bt, entity));
}

}  // namespace EffectTreeBuilder

NS_MG_END
