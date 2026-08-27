#include "mugen/bt/conditions/CondEffect.h"

#include "mugen/conf/Config.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/effect/Effect.h"

NS_MG_BEGIN

CondEffectAlive::CondEffectAlive() {}

CondEffectAlive::~CondEffectAlive() {}

CondEffectAttack::CondEffectAttack() {}

CondEffectAttack::~CondEffectAttack() {}

EffectHoldAction::EffectHoldAction() {}

EffectHoldAction::~EffectHoldAction() {}

bool CondEffectAlive::check(BTContext& ctx)
{
    auto* fx = Effect::of(ctx.entity);
    return fx && fx->isAlive();
}

bool CondEffectAttack::check(BTContext& ctx)
{
    auto* fx = Effect::of(ctx.entity);
    if (!fx || fx->effectId <= 0)
        return false;
    const auto* cfg = Config::getInstance()->getEffectConfigById(fx->effectId);
    if (!cfg)
        return false;
    for (int32_t aid : cfg->actionIds)
    {
        if (aid > 0)
            return true;
    }
    return false;
}

NS_MG_END
