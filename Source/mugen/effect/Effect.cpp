#include "mugen/effect/Effect.h"

#include "mugen/Components.h"
#include "mugen/combat/CombatHit.h"
#include "mugen/component/EffectComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/StdC.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"
#include "mugen/system/EffectLifeSystem.h"

#include <algorithm>

NS_MG_BEGIN

Effect* Effect::of(Entity* entity)
{
    if (!entity)
        return nullptr;
    auto* comp = MG_GET_COMPONENT(entity, EffectComponent);
    return comp ? comp->ensureEffect() : nullptr;
}

void Effect::bindFromConfig(const EffectConfig* cfg,
                            EntityId owner,
                            int32_t skillHitLookupId,
                            bool chainFromParent,
                            const TransformComponent* originTf)
{
    if (!cfg)
        return;
    (void)chainFromParent;
    effectId         = cfg->id;
    skillHitId       = skillHitLookupId > 0 ? skillHitLookupId : (cfg->hitId > 0 ? cfg->hitId : 0);
    ownerId          = owner;
    chainSpawned     = false;
    autoRelease      = cfg->autoRelease;
    followMode       = cfg->follow;
    follow           = followMode != 0;
    allowNextEffect  = true;
    destroyRequested = false;
    // autoRelease 是枚举 0/1/2/3：寿命跟动作树 / destroyRequested，不做硬编码超时
    lifetimeMs       = 0;
    radius           = cfg->radius > 0 ? cfg->radius : 40.0f;
    relativePosition = cfg->relativePosition;
    moveVx           = 0.0f;
    moveVy           = 0.0f;
    hitCount         = 0;
    hitCooldownMs    = 0;
    hitEntityIds.clear();
    elapsedMs = 0;
    if (originTf && !follow && cfg->velocity != 0.0f)
    {
        const float facing = originTf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
        moveVx             = cfg->velocity * facing;
        moveVy             = 0.0f;
    }
}

void Effect::bindVisual(EntityId owner, bool followOwner, int32_t lifeMs, const Vector3f& rel)
{
    effectId         = 0;
    skillHitId       = 0;
    ownerId          = owner;
    follow           = followOwner;
    lifetimeMs       = lifeMs;
    relativePosition = rel;
    moveVx           = 0.0f;
    moveVy           = 0.0f;
    chainSpawned     = false;
    hitCount         = 0;
    hitCooldownMs    = 0;
    hitEntityIds.clear();
    elapsedMs        = 0;
    radius           = 40.0f;
    baseSkillId      = 0;
    slotIndex        = 0;
    autoRelease      = 0;
    followMode       = followOwner ? 1 : 0;
    allowNextEffect  = true;
    destroyRequested = false;
}

bool Effect::isAlive() const
{
    if (destroyRequested)
        return false;
    return lifetimeMs <= 0 || elapsedMs < lifetimeMs;
}

bool Effect::update(Entity* entity, int32_t dtMs)
{
    if (!entity)
        return false;
    auto* ecs = entity->getECSManager();
    auto* tf  = MG_GET_COMPONENT(entity, TransformComponent);
    if (!ecs || !tf)
        return false;

    if (follow && ownerId != INVALID_ENTITY_ID)
    {
        if (auto* owner = ecs->getEntity(ownerId))
        {
            if (auto* ownerTf = MG_GET_COMPONENT(owner, TransformComponent))
            {
                const float facing  = ownerTf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
                tf->position.x      = ownerTf->position.x + static_cast<int32_t>(relativePosition.x * facing);
                tf->facingDirection = ownerTf->facingDirection;
                if (followMode == 2)
                {
                    tf->position.y = ownerTf->position.y + static_cast<int32_t>(relativePosition.y);
                    tf->position.z = static_cast<int32_t>(relativePosition.z);
                }
                else
                {
                    tf->position.y = ownerTf->position.y + static_cast<int32_t>(relativePosition.y);
                    tf->position.z = ownerTf->position.z + static_cast<int32_t>(relativePosition.z);
                }
            }
        }
    }
    else if (!follow)
    {
        if (moveVx == 0.0f && moveVy == 0.0f && effectId > 0)
        {
            if (const auto* cfg = Config::getInstance()->getEffectConfigById(effectId))
            {
                if (cfg->velocity != 0.0f)
                {
                    const float facing = tf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
                    moveVx             = cfg->velocity * facing;
                }
            }
        }
        if (moveVx != 0.0f || moveVy != 0.0f)
        {
            const float sec = dtMs / 1000.0f;
            tf->position.x += static_cast<int32_t>(moveVx * sec);
            tf->position.y += static_cast<int32_t>(moveVy * sec);
        }
    }

    elapsedMs += dtMs;
    const bool expired = destroyRequested || (lifetimeMs > 0 && elapsedMs >= lifetimeMs);
    if (expired)
    {
        if (!chainSpawned && allowNextEffect && effectId > 0)
        {
            const auto* cfg = Config::getInstance()->getEffectConfigById(effectId);
            if (cfg && cfg->nextEffectId > 0)
            {
                chainSpawned = true;
                EffectLifeSystem::spawnEffect(ecs, cfg->nextEffectId, ownerId, tf, skillHitId, true);
            }
        }
        return true;
    }
    return false;
}

void Effect::tryHit(Entity* entity, int32_t dtMs, Random& rng, const std::vector<Entity*>& defenders)
{
    if (!entity || skillHitId <= 0)
        return;
    auto* ecs = entity->getECSManager();
    auto* tf  = MG_GET_COMPONENT(entity, TransformComponent);
    if (!ecs || !tf)
        return;

    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
    {
        if (attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0)
            return;
    }

    if (hitCooldownMs > 0)
    {
        hitCooldownMs = (std::max)(0, hitCooldownMs - dtMs);
        return;
    }

    Entity* owner = ownerId != INVALID_ENTITY_ID ? ecs->getEntity(ownerId) : entity;
    if (!owner)
        owner = entity;

    const auto* hitTable    = Config::getInstance()->getSkillHitTableConfigById(skillHitId);
    const auto* effectCfg   = effectId > 0 ? Config::getInstance()->getEffectConfigById(effectId) : nullptr;
    const int32_t hitTarget = effectCfg ? effectCfg->hitTarget : 0;

    std::vector<DamageBox> attackBoxes;
    if (auto* avatar = MG_GET_COMPONENT(entity, AvatarComponent))
        attackBoxes = avatar->getAttackBoxes();
    if (attackBoxes.empty())
        attackBoxes.push_back(CombatHit::makeRadiusAttackBox(tf, radius));

    hitCount                  = 0;
    const int32_t maxHits     = hitTable && hitTable->hitCounts > 0 ? hitTable->hitCounts : 0;
    const int32_t hitInterval = hitTable ? hitTable->hitInterval : 80;

    for (Entity* defender : defenders)
    {
        if (!defender || defender == owner || defender == entity)
            continue;

        const bool hostile = CombatHit::isHostile(owner, defender);
        if (!CombatHit::passHitTarget(hitTarget, hostile))
            continue;

        auto* avatarB = MG_GET_COMPONENT(defender, AvatarComponent);
        if (!avatarB || avatarB->getDamageBoxes().empty())
            continue;

        if (hitInterval < 0)
        {
            bool seen = false;
            for (uint32_t id : hitEntityIds)
            {
                if (id == defender->getId())
                {
                    seen = true;
                    break;
                }
            }
            if (seen)
                continue;
        }

        if (!CombatHit::boxesOverlap(attackBoxes, avatarB->getDamageBoxes()))
            continue;

        const bool hitMustFx = hitTable && hitTable->hitMust == 1;
        if (effectCfg && !effectCfg->hitExtraControl.empty() && !hitMustFx)
        {
            if (effectCfg->hitExtraControl.size() > 1 && effectCfg->hitExtraControl[1] == 0)
            {
                auto* beh = MG_GET_COMPONENT(defender, BehaviorComponent);
                if (beh && beh->currentKind == static_cast<int32_t>(BehaviorKind::kGetUp))
                    continue;
            }
        }

        if (!hostile)
        {
            bool seen = false;
            for (uint32_t id : hitEntityIds)
            {
                if (id == defender->getId())
                {
                    seen = true;
                    break;
                }
            }
            if (seen)
                continue;
            hitEntityIds.push_back(defender->getId());
            CombatHit::applyEffectContactBuffs(entity, defender, false, false);
            continue;
        }

        ++hitCount;
        if (hitInterval < 0)
            hitEntityIds.push_back(defender->getId());
        else
            hitCooldownMs = hitInterval > 0 ? hitInterval : 80;

        CombatHit::applyHit(owner, defender, entity, hitTable, skillHitId, rng, ecs,
                            effectCfg ? &effectCfg->hitExtraControl : nullptr);

        if (autoRelease == 2 || autoRelease == 3)
        {
            if (hitInterval < 0 && maxHits > 0 &&
                (hitCount >= maxHits || static_cast<int32_t>(hitEntityIds.size()) >= maxHits))
            {
                destroyRequested = true;
                allowNextEffect  = (autoRelease == 3);
            }
        }

        if (hitInterval >= 0 && maxHits > 0 && hitCount >= maxHits)
            break;
    }
}

NS_MG_END
