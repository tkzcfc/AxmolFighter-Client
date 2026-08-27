#include "mugen/ai/SkillAi.h"

#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/StdC.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"
#include "mugen/skill/SkillManager.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace
{

// 参照 skill_ai 的 statusIndex：1=Hit 2=HitUp 3=HitDown 4=HitFloor 5=Wake 6=Attack。
// 0=Normal（不命中 1-6 的状态检查）；-1 在 config 侧表示不检查。
int32_t statusIndexOf(int32_t kind)
{
    switch (static_cast<BehaviorKind>(kind))
    {
    case BehaviorKind::kStun:
    case BehaviorKind::kHitSwitch:
        return 1;
    case BehaviorKind::kHitUp:
        return 2;
    case BehaviorKind::kHitDown:
        return 3;
    case BehaviorKind::kHitFloor:
        return 4;
    case BehaviorKind::kGetUp:
    case BehaviorKind::kWake:
        return 5;
    case BehaviorKind::kAttack:
        return 6;
    default:
        return 0;
    }
}

int32_t entityStatusIndex(Entity* entity)
{
    if (!entity)
        return 0;
    auto* behavior = MG_GET_COMPONENT(entity, BehaviorComponent);
    return behavior ? statusIndexOf(behavior->currentKind) : 0;
}

bool inRange(int32_t v, int32_t lo, int32_t hi)
{
    if (lo <= 0 && hi <= 0)
        return true;
    if (lo > 0 && v < lo)
        return false;
    if (hi > 0 && v > hi)
        return false;
    return true;
}

}  // namespace

void SkillAi::ensure(const SkillAiConfig* cfg)
{
    if (!cfg)
        return;
    skillAiId       = cfg->id;
    remainUseCount  = cfg->useCount;
    loadCdRemainMs  = cfg->loadCd;
    checkCdRemainMs = 0;
}

void SkillAi::tick(int32_t dtMs)
{
    if (loadCdRemainMs > 0)
        loadCdRemainMs = (std::max)(0, loadCdRemainMs - dtMs);
    if (checkCdRemainMs > 0)
        checkCdRemainMs = (std::max)(0, checkCdRemainMs - dtMs);
}

namespace
{

// 单条 composition 检查（1-7），返回是否通过。target 可为空（oppXxx 检查会失败）。
bool checkOppDisX(Entity* self, Entity* target, const Vector2i& scope)
{
    if (scope.x <= 0 && scope.y <= 0)
        return true;
    if (!self || !target)
        return false;
    auto* a = MG_GET_COMPONENT(self, TransformComponent);
    auto* b = MG_GET_COMPONENT(target, TransformComponent);
    if (!a || !b)
        return false;
    const int dx = std::abs(a->position.x - b->position.x);
    return inRange(dx, scope.x, scope.y);
}

bool checkOppDisZ(Entity* self, Entity* target, const Vector2i& scope)
{
    if (scope.x <= 0 && scope.y <= 0)
        return true;
    if (!self || !target)
        return false;
    auto* a = MG_GET_COMPONENT(self, TransformComponent);
    auto* b = MG_GET_COMPONENT(target, TransformComponent);
    if (!a || !b)
        return false;
    const int dz = std::abs(a->position.y - b->position.y);
    return inRange(dz, scope.x, scope.y);
}

bool checkOppStatus(Entity* /*self*/, Entity* target, int32_t oppStatus)
{
    if (oppStatus < 0)
        return true;
    return entityStatusIndex(target) == oppStatus;
}

bool checkOppCombo(Entity* /*self*/, Entity* target, int32_t oppCombo)
{
    if (oppCombo < 0)
        return true;
    auto* mgr = target ? SkillManager::of(target) : nullptr;
    if (!mgr || mgr->activeSkillAttackId <= 0)
        return false;
    return mgr->activeSlotIndex > 1;
}

bool checkOppSkillId(Entity* /*self*/, Entity* target, int32_t oppSkillId)
{
    if (oppSkillId < 0)
        return true;
    auto* mgr = target ? SkillManager::of(target) : nullptr;
    if (!mgr || mgr->activeSkillAttackId <= 0)
        return false;
    return mgr->activeSkillAttackId == oppSkillId;
}

bool checkSelfHp(Entity* self, const Vector2i& scope)
{
    if (scope.x <= 0 && scope.y <= 0)
        return true;
    auto* attr = MG_GET_COMPONENT(self, AttributeComponent);
    if (!attr || attr->basic.hpMax <= 0.0f)
        return true;
    const float per = 100.0f * attr->hp / attr->basic.hpMax;
    return inRange(static_cast<int32_t>(per + 0.5f), scope.x, scope.y);
}

bool checkSelfStatus(Entity* self, int32_t selfStatus)
{
    if (selfStatus < 0)
        return true;
    return entityStatusIndex(self) == selfStatus;
}

// 按 composition 行里的 index 跑对应检查。index 1-7 对应参照 checkFunc 表。
bool runCompositionCheck(int32_t index, Entity* self, Entity* target, const SkillAiConfig* cfg)
{
    switch (index)
    {
    case 1:
        return checkOppDisX(self, target, cfg->oppDisX);
    case 2:
        return checkOppDisZ(self, target, cfg->oppDisZ);
    case 3:
        return checkOppStatus(self, target, cfg->oppStatus);
    case 4:
        return checkOppCombo(self, target, cfg->oppCombo);
    case 5:
        return checkOppSkillId(self, target, cfg->oppSkillId);
    case 6:
        return checkSelfHp(self, cfg->selfHp);
    case 7:
        return checkSelfStatus(self, cfg->selfStatus);
    default:
        return true;
    }
}

// composition[0]=AND 组（全过才过）；composition[1]=OR 组（任一过或含 -1 即过）。
// 两个组都过才算 composition 过。缺行视为空：AND 空=vacuous true；OR 空=false。
bool checkComposition(Entity* self, Entity* target, const SkillAiConfig* cfg)
{
    bool andOk = true;
    if (cfg->composition.size() >= 1)
    {
        for (int32_t idx : cfg->composition[0].values)
        {
            if (!runCompositionCheck(idx, self, target, cfg))
            {
                andOk = false;
                break;
            }
        }
    }
    if (!andOk)
        return false;

    if (cfg->composition.size() < 2)
        return false;
    for (int32_t idx : cfg->composition[1].values)
    {
        if (idx == -1)
            return true;
        if (runCompositionCheck(idx, self, target, cfg))
            return true;
    }
    return false;
}

}  // namespace

bool SkillAi::check(Entity* self, Entity* target, Random& rng)
{
    if (!self)
        return false;

    if (loadCdRemainMs > 0)
        return false;

    if (remainUseCount == 0)
        return false;

    if (checkCdRemainMs > 0)
        return false;

    const auto* cfg = Config::getInstance()->getSkillAiConfigById(skillAiId);
    if (!cfg)
        return false;

    if (!checkComposition(self, target, cfg))
        return false;

    checkCdRemainMs = cfg->checkCd;

    if (cfg->prob < 100 && rng.nextFloat(0.0f, 100.0f) >= static_cast<float>(cfg->prob))
        return false;

    if (remainUseCount > 0)
        --remainUseCount;

    return true;
}

NS_MG_END
