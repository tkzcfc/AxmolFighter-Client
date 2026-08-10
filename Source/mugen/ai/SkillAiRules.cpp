#include "mugen/ai/SkillAiRules.h"

#include "mugen/Components.h"
#include "mugen/component/AIComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/TableConfig.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace SkillAiRules
{

namespace
{
// StatusIndex → 行为语义（参照 SkillAi.StatusIndex）
bool matchStatusIndex(Entity* entity, int32_t statusIndex)
{
    if (statusIndex < 0)
        return true;
    if (!entity)
        return false;

    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    auto* cast     = MG_GET_COMPONENT(entity, SkillCastComponent);
    if (!behavior)
        return false;

    const int32_t kind = behavior->currentKind;
    switch (statusIndex)
    {
    case 1:  // Hit
        return (behavior->statusTags & StateTag::kTagHitState) != 0 &&
               kind == static_cast<int32_t>(BehaviorKind::kStun);
    case 2:  // HitUp
        return kind == static_cast<int32_t>(BehaviorKind::kHitUp);
    case 3:  // HitDown
        return kind == static_cast<int32_t>(BehaviorKind::kHitDown);
    case 4:  // HitFloor
        return kind == static_cast<int32_t>(BehaviorKind::kHitFloor);
    case 5:  // Wake / GetUp
        return kind == static_cast<int32_t>(BehaviorKind::kGetUp);
    case 6:  // Attack
        return (cast && cast->activeSkillAttackId > 0) ||
               kind == static_cast<int32_t>(BehaviorKind::kAttack);
    default:
        return false;
    }
}

bool checkOppDisX(Entity* self, Entity* target, const SkillAiConfig* cfg)
{
    if (!self || !target || !cfg)
        return false;
    auto* a = MG_GET_COMPONENT(self, TransformComponent);
    auto* b = MG_GET_COMPONENT(target, TransformComponent);
    if (!a || !b)
        return false;
    const int ox = std::abs(a->position.x - b->position.x);
    return cfg->oppDisX.x <= ox && ox <= cfg->oppDisX.y;
}

bool checkOppDisZ(Entity* self, Entity* target, const SkillAiConfig* cfg)
{
    if (!self || !target || !cfg)
        return false;
    auto* a = MG_GET_COMPONENT(self, TransformComponent);
    auto* b = MG_GET_COMPONENT(target, TransformComponent);
    if (!a || !b)
        return false;
    const int oz = std::abs(a->position.y - b->position.y);
    return cfg->oppDisZ.x <= oz && oz <= cfg->oppDisZ.y;
}

bool checkOppStatus(Entity* /*self*/, Entity* target, const SkillAiConfig* cfg)
{
    if (!cfg)
        return false;
    return matchStatusIndex(target, cfg->oppStatus);
}

bool checkOppCombo(Entity* /*self*/, Entity* target, const SkillAiConfig* cfg)
{
    if (!cfg || cfg->oppCombo < 0)
        return true;
    if (!target)
        return false;
    auto* cast = MG_GET_COMPONENT(target, SkillCastComponent);
    if (!cast || cast->activeSkillAttackId <= 0)
        return false;
    // 参照：对方连段 slotIndex > 1
    return cast->activeStepInSlot > 0;
}

bool checkOppSkillId(Entity* /*self*/, Entity* target, const SkillAiConfig* cfg)
{
    if (!cfg || cfg->oppSkillId < 0)
        return true;
    if (!target)
        return false;
    auto* cast = MG_GET_COMPONENT(target, SkillCastComponent);
    return cast && cast->activeSkillAttackId == cfg->oppSkillId;
}

bool checkSelfHp(Entity* self, Entity* /*target*/, const SkillAiConfig* cfg)
{
    if (!self || !cfg)
        return false;
    auto* attr = MG_GET_COMPONENT(self, AttributeComponent);
    if (!attr || attr->currentAttribute.hpMax <= 0)
        return false;
    const int per = static_cast<int>(100.0f * attr->currentAttribute.hp /
                                     static_cast<float>(attr->currentAttribute.hpMax));
    return cfg->selfHp.x <= per && per <= cfg->selfHp.y;
}

bool checkSelfStatus(Entity* self, Entity* /*target*/, const SkillAiConfig* cfg)
{
    if (!cfg)
        return false;
    return matchStatusIndex(self, cfg->selfStatus);
}

using CheckFn = bool (*)(Entity*, Entity*, const SkillAiConfig*);

CheckFn checkFnByIndex(int32_t index)
{
    switch (index)
    {
    case 1:
        return &checkOppDisX;
    case 2:
        return &checkOppDisZ;
    case 3:
        return &checkOppStatus;
    case 4:
        return &checkOppCombo;
    case 5:
        return &checkOppSkillId;
    case 6:
        return &checkSelfHp;
    case 7:
        return &checkSelfStatus;
    default:
        return nullptr;
    }
}

bool runCheckIndex(int32_t index, Entity* self, Entity* target, const SkillAiConfig* cfg)
{
    if (auto* fn = checkFnByIndex(index))
        return fn(self, target, cfg);
    return false;
}

bool checkComposition(Entity* self, Entity* target, const SkillAiConfig* cfg)
{
    if (!cfg)
        return false;

    // composition[0] = AND（全部通过）；composition[1] = OR（任一通过，-1 恒真）
    bool andOk = true;
    if (!cfg->composition.empty())
    {
        for (int32_t index : cfg->composition[0].values)
        {
            if (!runCheckIndex(index, self, target, cfg))
            {
                andOk = false;
                break;
            }
        }
    }

    bool orOk = false;
    if (cfg->composition.size() >= 2)
    {
        for (int32_t index : cfg->composition[1].values)
        {
            if (index < 0 || runCheckIndex(index, self, target, cfg))
            {
                orOk = true;
                break;
            }
        }
    }

    return andOk && orOk;
}

}  // namespace

void ensureRuntime(SkillAiRuntimeState& state, const SkillAiConfig* cfg)
{
    if (!cfg)
        return;
    if (state.skillAiId != cfg->id)
    {
        state.skillAiId       = cfg->id;
        state.remainUseCount  = cfg->useCount;
        state.loadCdRemainMs  = cfg->loadCd;
        state.checkCdRemainMs = 0;
    }
}

void tick(SkillAiRuntimeState& state, int32_t dtMs)
{
    if (dtMs <= 0)
        return;
    if (state.loadCdRemainMs > 0)
        state.loadCdRemainMs = (std::max)(0, state.loadCdRemainMs - dtMs);
    if (state.checkCdRemainMs > 0)
        state.checkCdRemainMs = (std::max)(0, state.checkCdRemainMs - dtMs);
}

bool check(Entity* self,
           Entity* target,
           SkillAiRuntimeState& state,
           const SkillAiConfig* cfg,
           Random& rng)
{
    if (!self || !cfg)
        return false;

    ensureRuntime(state, cfg);

    if (state.loadCdRemainMs > 0)
        return false;
    if (state.remainUseCount == 0)
        return false;
    if (state.checkCdRemainMs > 0)
        return false;
    if (!checkComposition(self, target, cfg))
        return false;

    state.checkCdRemainMs = cfg->checkCd;

    // GRandomF(0,100) < prob
    if (rng.nextInt(0, 100) >= cfg->prob)
        return false;

    if (state.remainUseCount > 0)
        --state.remainUseCount;

    return true;
}

}  // namespace SkillAiRules

NS_MG_END
