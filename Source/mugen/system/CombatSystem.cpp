#include "CombatSystem.h"

#include "mugen/GameWord.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

NS_MG_BEGIN

namespace
{
struct HitRuntime
{
    int32_t skillId    = 0;
    int32_t count      = 0;
    int32_t cooldownMs = 0;
};

std::unordered_map<uint64_t, HitRuntime> s_behaviorHits;

uint64_t hitKey(EntityId attacker, EntityId target)
{
    return (static_cast<uint64_t>(attacker) << 32) ^ static_cast<uint32_t>(target);
}

DamageBox makeRadiusAttackBox(const TransformComponent* tf, float radius)
{
    DamageBox box;
    const int32_t r = static_cast<int32_t>(std::max(20.0f, radius));
    const int32_t w = r * 2;
    const int32_t d = r;
    const int32_t h = r * 2;
    if (tf && tf->facingDirection == FacingDirection::kFacingLeft)
        box.pos.x = tf->position.x - w;
    else
        box.pos.x = tf ? tf->position.x : 0;
    box.pos.y  = tf ? tf->position.y - d / 2 : 0;
    box.pos.z  = tf ? tf->position.z : 0;
    box.size.x = w;
    box.size.y = d;
    box.size.z = h;
    return box;
}

void applyBehaviorHit(Entity* attacker,
                      Entity* defender,
                      int32_t skillId,
                      float hurtRate,
                      int32_t displacementId,
                      int32_t hitType,
                      int32_t hitstun,
                      float impulseX,
                      float impulseZ)
{
    int32_t finalHitstunMs = 0;
    if (hitstun > 0)
    {
        auto attrCompB      = MG_GET_COMPONENT(defender, AttributeComponent);
        int32_t hitRecovery = attrCompB ? static_cast<int32_t>(attrCompB->currentAttribute.hitRecovery) : 0;
        float resistRatio   = std::max(0.0f, std::min(100.0f, static_cast<float>(hitRecovery))) / 100.0f;
        finalHitstunMs      = std::max(HITSTUN_MIN_MS, static_cast<int32_t>(hitstun * (1.0f - resistRatio)));
    }

    auto* attrCompA = MG_GET_COMPONENT(attacker, AttributeComponent);
    auto* attrCompB = MG_GET_COMPONENT(defender, AttributeComponent);
    if (attrCompB)
    {
        const auto* hurt               = Config::getInstance()->getSkillHurtConfigById(skillId);
        float damage                   = hurt && hurt->hurt > 0 ? static_cast<float>(hurt->hurt)
                                                                : (attrCompA ? attrCompA->currentAttribute.physicalAttack : 0.0f);
        attrCompB->currentAttribute.hp = std::max(0.0f, attrCompB->currentAttribute.hp - damage * hurtRate);
    }

    if (displacementId > 0)
    {
        if (auto* disp = MG_GET_COMPONENT(defender, DisplacementComponent))
            if (auto* d = Config::getInstance()->getDisplacementConfigById(displacementId))
                disp->start(d);
    }

    if (auto* skillStateCompB = MG_GET_COMPONENT(defender, SkillStateComponent))
    {
        PendingHitInfo hitInfo;
        hitInfo.attackerId = attacker->getId();
        hitInfo.hitType    = static_cast<HitType>(hitType);
        hitInfo.hitState   = "Stun";
        hitInfo.hitstunMs  = finalHitstunMs;
        hitInfo.impulseX   = impulseX;
        hitInfo.impulseZ   = impulseZ;
        skillStateCompB->pendingHits.emplace_back(hitInfo);
    }
}

bool boxesOverlap(const std::vector<DamageBox>& attackBoxes, const std::vector<DamageBox>& damageBoxes)
{
    for (const auto& attackBox : attackBoxes)
        for (const auto& damageBox : damageBoxes)
            if (attackBox.overlaps(damageBox))
                return true;
    return false;
}
}  // namespace

CombatSystem::CombatSystem() {}

CombatSystem::~CombatSystem() {}

void CombatSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, AvatarComponent);
}

void CombatSystem::onEntityAdded(Entity* entity) {}

void CombatSystem::onEntityRemoved(Entity* entity) {}

void CombatSystem::update()
{
    auto* ecs          = getECSManager();
    const int32_t dtMs = ecs->getLastUpdateTimeMs();

    for (size_t i = 0; i < entities.size(); ++i)
    {
        auto entityA     = entities[i];
        auto avatarCompA = MG_GET_COMPONENT(entityA, AvatarComponent);
        auto* behaviorA  = MG_GET_COMPONENT(entityA, BehaviorComponent);

        if (behaviorA && behaviorA->activeSkillAttackId <= 0)
        {
            const uint64_t attackerKey = static_cast<uint64_t>(entityA->getId()) << 32;
            for (auto it = s_behaviorHits.begin(); it != s_behaviorHits.end();)
                it = (it->first & 0xffffffff00000000ULL) == attackerKey ? s_behaviorHits.erase(it) : ++it;
        }

        std::vector<DamageBox> attackBoxes = avatarCompA->getAttackBoxes();
        if (attackBoxes.empty() && behaviorA && behaviorA->activeSkillAttackId > 0)
        {
            float radius = 40.0f;
            if (behaviorA->roleConfig && behaviorA->roleConfig->radius > 0)
                radius = behaviorA->roleConfig->radius;
            attackBoxes.push_back(makeRadiusAttackBox(MG_GET_COMPONENT(entityA, TransformComponent), radius));
        }
        if (attackBoxes.empty())
            continue;

        const auto* hitTable = behaviorA && behaviorA->activeSkillAttackId > 0
                                   ? Config::getInstance()->getSkillHitTableConfigById(behaviorA->activeSkillAttackId)
                                   : nullptr;
        int32_t skillId      = behaviorA ? behaviorA->activeSkillAttackId : 0;

        if (!behaviorA || skillId <= 0)
            continue;
        if (!hitTable || hitTable->hitType == 0)
        {
            // skill_hit 缺失时仍允许轻击
        }

        for (size_t j = 0; j < entities.size(); ++j)
        {
            if (i == j)
                continue;

            auto entityB      = entities[j];
            EntityId targetId = entityB->getId();

            HitRuntime* runtime = nullptr;
            if (behaviorA && skillId > 0)
            {
                runtime = &s_behaviorHits[hitKey(entityA->getId(), targetId)];
                if (runtime->skillId != skillId)
                    *runtime = HitRuntime{skillId, 0, 0};
                if (runtime->cooldownMs > 0)
                    continue;
                const int32_t maxHits = hitTable && hitTable->hitCounts > 0 ? hitTable->hitCounts : 1;
                if (runtime->count >= maxHits)
                    continue;
            }
            auto avatarCompB        = MG_GET_COMPONENT(entityB, AvatarComponent);
            const auto& damageBoxes = avatarCompB->getDamageBoxes();
            if (damageBoxes.empty())
                continue;
            if (!boxesOverlap(attackBoxes, damageBoxes))
                continue;

            if (runtime)
            {
                ++runtime->count;
                runtime->cooldownMs = hitTable && hitTable->hitInterval > 0 ? hitTable->hitInterval : 0;
            }
            auto attrCompA         = MG_GET_COMPONENT(entityA, AttributeComponent);
            float impulseX         = 0.0f;
            float impulseZ         = 0.0f;
            int32_t hitType        = static_cast<int32_t>(HitType::kHitLight);
            int32_t hitstun        = 250;
            float hurtRate         = 1.0f;
            int32_t displacementId = -1;
            if (hitTable)
            {
                hitType = hitTable->hitType <= 0 ? static_cast<int32_t>(HitType::kHitLight) : hitTable->hitType;
                hitstun = std::max(hitTable->stiffTime, hitTable->freezeTime);
                if (hitstun <= 0)
                    hitstun = hitTable->hitRigidity > 0 ? hitTable->hitRigidity : 250;
                hurtRate       = hitTable->hurtRate > 0 ? hitTable->hurtRate : 1.0f;
                displacementId = hitTable->displacementId;
            }

            applyBehaviorHit(entityA, entityB, skillId, hurtRate, displacementId, hitType, hitstun, impulseX, impulseZ);
        }
    }

    // 特效实体攻击盒
    Signature effectSig;
    effectSig.set(ecs->getComponentTypeId("EffectLifetimeComponent"));
    auto effectEntities = ecs->getEntitiesBySignature(effectSig);
    for (Entity* effectEntity : effectEntities)
    {
        auto* fx = MG_GET_COMPONENT(effectEntity, EffectLifetimeComponent);
        auto* tf = MG_GET_COMPONENT(effectEntity, TransformComponent);
        if (!fx || !tf || fx->skillHitId <= 0)
            continue;
        Entity* owner = fx->ownerId != INVALID_ENTITY_ID ? ecs->getEntity(fx->ownerId) : effectEntity;
        if (!owner)
            owner = effectEntity;

        const auto* hitTable = Config::getInstance()->getSkillHitTableConfigById(fx->skillHitId);
        std::vector<DamageBox> attackBoxes{makeRadiusAttackBox(tf, fx->radius)};

        for (Entity* defender : entities)
        {
            if (defender == owner || defender == effectEntity)
                continue;
            auto* avatarB = MG_GET_COMPONENT(defender, AvatarComponent);
            if (!avatarB || avatarB->getDamageBoxes().empty())
                continue;

            HitRuntime* runtime = &s_behaviorHits[hitKey(effectEntity->getId(), defender->getId())];
            if (runtime->skillId != fx->skillHitId)
                *runtime = HitRuntime{fx->skillHitId, 0, 0};
            if (runtime->cooldownMs > 0)
                continue;
            const int32_t maxHits = hitTable && hitTable->hitCounts > 0 ? hitTable->hitCounts : 1;
            if (runtime->count >= maxHits)
                continue;
            if (!boxesOverlap(attackBoxes, avatarB->getDamageBoxes()))
                continue;

            ++runtime->count;
            runtime->cooldownMs = hitTable && hitTable->hitInterval > 0 ? hitTable->hitInterval : 80;

            int32_t hitType =
                hitTable && hitTable->hitType > 0 ? hitTable->hitType : static_cast<int32_t>(HitType::kHitLight);
            int32_t hitstun =
                hitTable ? std::max({hitTable->stiffTime, hitTable->freezeTime, hitTable->hitRigidity}) : 250;
            if (hitstun <= 0)
                hitstun = 250;
            float hurtRate         = hitTable && hitTable->hurtRate > 0 ? hitTable->hurtRate : 1.0f;
            int32_t displacementId = hitTable ? hitTable->displacementId : -1;
            applyBehaviorHit(owner, defender, fx->skillHitId, hurtRate, displacementId, hitType, hitstun, 0.0f, 0.0f);
        }
    }

    for (auto& entry : s_behaviorHits)
        if (entry.second.cooldownMs > 0)
            entry.second.cooldownMs = std::max(0, entry.second.cooldownMs - dtMs);
}

NS_MG_END
