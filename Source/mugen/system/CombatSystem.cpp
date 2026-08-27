#include "CombatSystem.h"

#include "mugen/GameWord.h"
#include "mugen/Components.h"
#include "mugen/combat/CombatHit.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/math/Random.h"
#include "mugen/effect/Effect.h"
#include "mugen/skill/SkillManager.h"

#include <algorithm>
#include <cmath>
#include <vector>

NS_MG_BEGIN

namespace
{

// 近战 hitTarget：当前技能第一段特效的 hitTarget；无特效则敌方。
int32_t resolveMeleeHitTarget(Entity* attacker)
{
    auto* mgr = SkillManager::of(attacker);
    if (!mgr || mgr->activeSkillAttackId <= 0)
        return 0;
    const auto* skill = Config::getInstance()->getSkillAttackConfigById(mgr->activeSkillAttackId);
    if (!skill || skill->actionIds.empty())
        return 0;
    int32_t toward = mgr->towardIndex > 0 ? mgr->towardIndex : 1;
    size_t row     = static_cast<size_t>(toward - 1);
    if (row >= skill->actionIds.size())
        row = 0;
    const auto& ids = skill->actionIds[row].values;
    if (ids.empty())
        return 0;
    const auto* action = Config::getInstance()->getActionAttackConfigById(ids.front());
    if (!action || action->effectIds.empty() || action->effectIds.front() <= 0)
        return 0;
    const auto* fx = Config::getInstance()->getEffectConfigById(action->effectIds.front());
    return fx ? fx->hitTarget : 0;
}

const std::vector<int32_t>* resolveMeleeExtraControl(Entity* attacker)
{
    auto* mgr = SkillManager::of(attacker);
    if (!mgr || mgr->activeSkillAttackId <= 0)
        return nullptr;
    const auto* skill = Config::getInstance()->getSkillAttackConfigById(mgr->activeSkillAttackId);
    if (!skill || skill->actionIds.empty())
        return nullptr;
    int32_t toward = mgr->towardIndex > 0 ? mgr->towardIndex : 1;
    size_t row     = static_cast<size_t>(toward - 1);
    if (row >= skill->actionIds.size())
        row = 0;
    const auto& ids = skill->actionIds[row].values;
    if (ids.empty())
        return nullptr;
    const auto* action = Config::getInstance()->getActionAttackConfigById(ids.front());
    if (!action || action->effectIds.empty() || action->effectIds.front() <= 0)
        return nullptr;
    const auto* fx = Config::getInstance()->getEffectConfigById(action->effectIds.front());
    return fx && !fx->hitExtraControl.empty() ? &fx->hitExtraControl : nullptr;
}

void resetMeleeHits(SkillManager* mgr, int32_t skillId)
{
    if (!mgr)
        return;
    if (skillId <= 0)
    {
        mgr->meleeHitSkillId = 0;
        mgr->meleeHitTargetIds.clear();
        mgr->meleeHitCounts.clear();
        mgr->meleeHitCooldowns.clear();
        return;
    }
    if (mgr->meleeHitSkillId != skillId)
    {
        mgr->meleeHitSkillId = skillId;
        mgr->meleeHitTargetIds.clear();
        mgr->meleeHitCounts.clear();
        mgr->meleeHitCooldowns.clear();
    }
}

int findMeleeHitIndex(SkillManager* mgr, EntityId targetId)
{
    if (!mgr)
        return -1;
    for (size_t i = 0; i < mgr->meleeHitTargetIds.size(); ++i)
    {
        if (mgr->meleeHitTargetIds[i] == static_cast<uint32_t>(targetId))
            return static_cast<int>(i);
    }
    return -1;
}

}  // namespace

CombatSystem::CombatSystem() {}
CombatSystem::~CombatSystem() {}

void CombatSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, AvatarComponent);
}

void CombatSystem::onEntityAdded(Entity* /*entity*/) {}
void CombatSystem::onEntityRemoved(Entity* /*entity*/) {}

void CombatSystem::update()
{
    auto* ecs = getECSManager();
    if (!ecs)
        return;
    const int32_t dtMs = ecs->getLastUpdateTimeMs();

    auto* word = reinterpret_cast<GameWord*>(ecs->getUserdata());
    Random localRng;
    Random& rng = word ? word->random : localRng;

    Signature attrSig;
    attrSig.set(ecs->getComponentTypeId("AttributeComponent"));
    for (Entity* e : ecs->getEntitiesBySignature(attrSig))
    {
        auto* attr = MG_GET_COMPONENT(e, AttributeComponent);
        if (!attr)
            continue;
        if (attr->freezeDelayMs > 0)
        {
            attr->freezeDelayMs = std::max(0, attr->freezeDelayMs - dtMs);
            continue;
        }
        if (attr->freezeRemainingMs > 0)
            attr->freezeRemainingMs = std::max(0, attr->freezeRemainingMs - dtMs);
    }

    for (size_t i = 0; i < entities.size(); ++i)
    {
        auto* entityA     = entities[i];
        auto* avatarCompA = MG_GET_COMPONENT(entityA, AvatarComponent);
        auto* behaviorA   = MG_GET_COMPONENT(entityA, BehaviorComponent);
        auto* mgrA        = SkillManager::of(entityA);
        auto* attrA       = MG_GET_COMPONENT(entityA, AttributeComponent);
        if (attrA && attrA->freezeRemainingMs > 0 && attrA->freezeDelayMs <= 0)
            continue;

        const int32_t skillId = mgrA ? mgrA->activeSkillAttackId : 0;

        if (skillId <= 0)
        {
            if (mgrA)
                resetMeleeHits(mgrA, 0);
            continue;
        }

        resetMeleeHits(mgrA, skillId);
        for (int32_t& cd : mgrA->meleeHitCooldowns)
            if (cd > 0)
                cd = (std::max)(0, cd - dtMs);

        std::vector<DamageBox> attackBoxes = avatarCompA->getAttackBoxes();
        if (attackBoxes.empty())
        {
            float radius = 40.0f;
            if (behaviorA && behaviorA->roleConfig && behaviorA->roleConfig->radius > 0)
                radius = behaviorA->roleConfig->radius;
            attackBoxes.push_back(
                CombatHit::makeRadiusAttackBox(MG_GET_COMPONENT(entityA, TransformComponent), radius));
        }
        if (attackBoxes.empty())
            continue;

        const auto* hitTable = Config::getInstance()->getSkillHitTableConfigById(skillId);

        for (size_t j = 0; j < entities.size(); ++j)
        {
            if (i == j)
                continue;
            auto* entityB = entities[j];
            auto* attrB   = MG_GET_COMPONENT(entityB, AttributeComponent);
            if (attrB && attrB->freezeRemainingMs > 0 && attrB->freezeDelayMs <= 0)
                continue;

            if (!CombatHit::passHitTarget(resolveMeleeHitTarget(entityA), CombatHit::isHostile(entityA, entityB)))
                continue;

            int idx = findMeleeHitIndex(mgrA, entityB->getId());
            if (idx >= 0 && mgrA->meleeHitCooldowns[static_cast<size_t>(idx)] > 0)
                continue;

            const int32_t hitInterval = hitTable ? hitTable->hitInterval : 0;
            const int32_t maxHits     = hitTable && hitTable->hitCounts > 0 ? hitTable->hitCounts : 1;
            const int32_t curCount    = idx >= 0 ? mgrA->meleeHitCounts[static_cast<size_t>(idx)] : 0;
            if (hitInterval < 0)
            {
                if (curCount >= 1)
                    continue;
            }
            else if (curCount >= maxHits)
            {
                continue;
            }

            auto* avatarCompB = MG_GET_COMPONENT(entityB, AvatarComponent);
            if (!avatarCompB || avatarCompB->getDamageBoxes().empty())
                continue;
            if (!CombatHit::boxesOverlap(attackBoxes, avatarCompB->getDamageBoxes()))
                continue;

            if (idx < 0)
            {
                mgrA->meleeHitTargetIds.push_back(static_cast<uint32_t>(entityB->getId()));
                mgrA->meleeHitCounts.push_back(1);
                mgrA->meleeHitCooldowns.push_back(hitInterval < 0 ? 0 : (hitInterval > 0 ? hitInterval : 0));
            }
            else
            {
                ++mgrA->meleeHitCounts[static_cast<size_t>(idx)];
                mgrA->meleeHitCooldowns[static_cast<size_t>(idx)] =
                    hitInterval < 0 ? 0 : (hitInterval > 0 ? hitInterval : 0);
            }

            CombatHit::applyHit(entityA, entityB, nullptr, hitTable, skillId, rng, ecs,
                                resolveMeleeExtraControl(entityA));
        }
    }

    Signature effectSig;
    effectSig.set(ecs->getComponentTypeId("EffectComponent"));
    for (Entity* effectEntity : ecs->getEntitiesBySignature(effectSig))
    {
        if (auto* fx = Effect::of(effectEntity))
            fx->tryHit(effectEntity, dtMs, rng, entities);
    }
}

NS_MG_END
