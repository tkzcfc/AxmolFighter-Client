#include "mugen/combat/CombatHit.h"

#include "mugen/Components.h"
#include "mugen/buff/BuffManager.h"
#include "mugen/buff/BuffRuleUtil.h"
#include "mugen/buff/ExtendAttribute.h"
#include "mugen/combat/DamageCalculator.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"
#include "mugen/effect/Effect.h"
#include "mugen/skill/SkillManager.h"
#include "mugen/system/EffectLifeSystem.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace
{

bool passHitCondition(Entity* defender, int32_t hitCondition)
{
    if (!defender || hitCondition < 0)
        return true;
    auto* behavior = MG_GET_COMPONENT(defender, BehaviorComponent);
    auto* physics  = MG_GET_COMPONENT(defender, PhysicsComponent);
    if (!behavior)
        return true;

    const bool down = (behavior->statusTags & StateTag::kTagDownState) != 0;
    const bool air  = (behavior->statusTags & StateTag::kTagAirborne) != 0 ||
                     (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitUp)) ||
                     (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitDown));
    const bool onGround = physics && physics->onGround;

    // 0 无法命中倒地；1 无法命中浮空；2 两者都不行
    if (hitCondition == 0)
        return !(down && onGround);
    if (hitCondition == 1)
        return !air;
    if (hitCondition == 2)
        return !(down && onGround) && !air;
    return true;
}

int32_t resolveLevel(Entity* entity)
{
    if (auto* data = MG_GET_COMPONENT(entity, ActorDataComponent))
        if (data->characterLevel > 0)
            return data->characterLevel;
    return 1;
}

bool isHeroEntity(Entity* entity)
{
    auto* id = MG_GET_COMPONENT(entity, IdentityComponent);
    return id && id->category == EntityCategory::kPlayer;
}

// 英雄用自身等级 hurt；召唤沿 belongEntityId 追到英雄；怪/无主人不加 standHurt。
const SkillHurtConfig* resolveAttackerHurtStd(Entity* attacker, ECSManager* ecs)
{
    Entity* e = attacker;
    for (int guard = 0; e && guard < 8; ++guard)
    {
        if (isHeroEntity(e))
        {
            return Config::getInstance()->getSkillHurtConfigById(
                DamageCalculator::hurtStandardId(resolveLevel(e), true));
        }
        auto* id = MG_GET_COMPONENT(e, IdentityComponent);
        if (!id || id->belongEntityId == INVALID_ENTITY_ID || !ecs)
            break;
        e = ecs->getEntity(id->belongEntityId);
    }
    return nullptr;
}

bool extraAllowsInvincible(const std::vector<int32_t>* extraControl)
{
    return extraControl && !extraControl->empty() && extraControl->front() != 0;
}

void applyFreeze(Entity* entity, int32_t freezeMs, int32_t delayMs)
{
    if (!entity || freezeMs <= 0)
        return;
    auto* attr = MG_GET_COMPONENT(entity, AttributeComponent);
    if (!attr)
        return;
    // 已在顿帧中：叠时长，不重新武装 delay（避免中途解冻）
    if (attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0)
    {
        attr->freezeRemainingMs = (std::max)(attr->freezeRemainingMs, freezeMs);
        return;
    }
    if (delayMs > 0)
    {
        attr->freezeDelayMs     = delayMs;
        attr->freezeRemainingMs = freezeMs;
    }
    else
    {
        attr->freezeDelayMs     = 0;
        attr->freezeRemainingMs = (std::max)(attr->freezeRemainingMs, freezeMs);
    }
}

}  // namespace

namespace CombatHit
{

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

bool boxesOverlap(const std::vector<DamageBox>& attackBoxes, const std::vector<DamageBox>& damageBoxes)
{
    for (const auto& attackBox : attackBoxes)
        for (const auto& damageBox : damageBoxes)
            if (attackBox.overlaps(damageBox))
                return true;
    return false;
}

bool isHostile(Entity* a, Entity* b)
{
    if (!a || !b)
        return false;
    auto* ia = MG_GET_COMPONENT(a, IdentityComponent);
    auto* ib = MG_GET_COMPONENT(b, IdentityComponent);
    if (!ia || !ib)
        return true;

    // 玩家 vs 怪物
    if (ia->category != ib->category)
    {
        const bool aCombatant = (ia->category == EntityCategory::kPlayer || ia->category == EntityCategory::kMonster);
        const bool bCombatant = (ib->category == EntityCategory::kPlayer || ib->category == EntityCategory::kMonster);
        if (aCombatant && bCombatant)
            return true;
    }

    // 同类别：monsterCamps 位有交集视为友军
    if (ia->monsterCamps != 0 && ib->monsterCamps != 0)
        return (ia->monsterCamps & ib->monsterCamps) == 0;

    // 同为玩家：友军
    if (ia->category == EntityCategory::kPlayer && ib->category == EntityCategory::kPlayer)
        return false;

    return ia->category != ib->category;
}

bool passHitTarget(int32_t hitTarget, bool hostile)
{
    if (hitTarget == 0)
        return hostile;
    if (hitTarget == 1)
        return !hostile;
    return true;
}

void applyHit(Entity* attacker,
              Entity* defender,
              Entity* effectEntity,
              const SkillHitTableConfig* hitTable,
              int32_t skillHitLookupId,
              Random& rng,
              ECSManager* ecs,
              const std::vector<int32_t>* extraControl)
{
    if (!attacker || !defender)
        return;

    if (auto* attrDead = MG_GET_COMPONENT(defender, AttributeComponent))
    {
        if (attrDead->hp <= 0.0f)
            return;
    }

    if (auto* behGate = MG_GET_COMPONENT(defender, BehaviorComponent))
    {
        const int32_t kind = behGate->currentKind;
        if (kind == static_cast<int32_t>(BehaviorKind::kDeath) || kind == static_cast<int32_t>(BehaviorKind::kGetUp) ||
            kind == static_cast<int32_t>(BehaviorKind::kWake))
            return;
    }

    const bool hitMustEarly = hitTable && hitTable->hitMust == 1;
    if (DamageCalculator::isInvincible(defender) && !hitMustEarly && !extraAllowsInvincible(extraControl))
        return;

    if (hitTable && !passHitCondition(defender, hitTable->hitCondition))
        return;

    if (auto* buffMgr = BuffManager::of(attacker))
        buffMgr->trigger(attacker, BFEvent::BeforeHit, defender, skillHitLookupId);
    if (auto* buffMgr = BuffManager::of(defender))
        buffMgr->trigger(defender, BFEvent::BeforeToBeHit, attacker, skillHitLookupId);

    auto* attrA = MG_GET_COMPONENT(attacker, AttributeComponent);
    auto* attrB = MG_GET_COMPONENT(defender, AttributeComponent);
    if (!attrB)
        return;

    const bool hitMust  = hitTable && hitTable->hitMust == 1;
    const int32_t defLv = resolveLevel(defender);
    const bool heroDef  = isHeroEntity(defender);
    const auto* hurtStd =
        Config::getInstance()->getSkillHurtConfigById(DamageCalculator::hurtStandardId(defLv, heroDef));
    const auto* atkHurtStd = resolveAttackerHurtStd(attacker, ecs);

    DamageInput din;
    din.attacker        = attrA;
    din.defender        = attrB;
    din.hitCfg          = hitTable;
    din.hurtStd         = hurtStd;
    din.attackerHurtStd = atkHurtStd;
    din.skillAddition   = attrA ? attrA->extendAttribute.get(ExtendAttributeType::SkillAddition) : 0.0f;
    din.hitMust         = hitMust;
    din.isHeroDefender  = heroDef;

    SkillHitTableConfig fallbackHit;
    if (!hitTable)
    {
        fallbackHit.hurtRate  = 1.0f;
        fallbackHit.hurtType  = 0;
        fallbackHit.hitType   = static_cast<int32_t>(HitType::kHitLight);
        fallbackHit.stiffTime = 250;
        din.hitCfg            = &fallbackHit;
    }

    DamageResult dmg = DamageCalculator::calculate(din, rng);
    if (dmg.isDodge)
    {
        if (auto* buffMgr = BuffManager::of(defender))
            buffMgr->trigger(defender, BFEvent::AttackMiss, attacker, skillHitLookupId);
        applyEffectContactBuffs(effectEntity, defender, true, true);
        return;
    }

    int32_t hitstun        = 250;
    int32_t tableHitType   = 0;
    HitType runtimeHitType = HitType::kHitLight;
    int32_t displacementId = -1;
    int32_t hitId          = 0;
    float impulseX         = 0.0f;
    float impulseZ         = 0.0f;
    int32_t freezeMs       = 0;
    int32_t freezeDelay    = 0;
    int32_t freezeRole     = 0;
    int32_t freezeFx       = 0;
    int32_t hitRigidity    = 0;
    float towardSign       = 1.0f;

    if (effectEntity)
    {
        if (auto* etf = MG_GET_COMPONENT(effectEntity, TransformComponent))
            towardSign = etf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;
    }
    else if (auto* atf = MG_GET_COMPONENT(attacker, TransformComponent))
        towardSign = atf->facingDirection == FacingDirection::kFacingLeft ? -1.0f : 1.0f;

    if (hitTable)
    {
        tableHitType = hitTable->hitType;
        hitId        = hitTable->id;
        hitstun      = hitTable->stiffTime > 0 ? hitTable->stiffTime : 0;
        if (hitstun <= 0)
            hitstun = hitTable->hitRigidity > 0 ? hitTable->hitRigidity : 250;
        hitRigidity    = hitTable->hitRigidity;
        displacementId = hitTable->displacementId;
        freezeMs       = hitTable->freezeTime;
        freezeDelay    = hitTable->freezeTimeDelay;
        freezeRole     = hitTable->freezeTimeControlRole;
        freezeFx       = hitTable->freezeTimeControlEffect;

        auto* behaviorDef     = MG_GET_COMPONENT(defender, BehaviorComponent);
        const int32_t defKind = behaviorDef ? behaviorDef->currentKind : 0;
        const int32_t roleDisp =
            (behaviorDef && behaviorDef->roleConfig) ? behaviorDef->roleConfig->hitDisplacementId : -1;
        if (roleDisp == 0)
            displacementId = -1;
        else if (roleDisp > 0)
        {
            displacementId = roleDisp;
            if (tableHitType == 0)
                displacementId = hitTable->displacementId;
        }

        const bool useAirDisp = tableHitType == 0 && (defKind == static_cast<int32_t>(BehaviorKind::kHitUp) ||
                                                      defKind == static_cast<int32_t>(BehaviorKind::kHitDown) ||
                                                      defKind == static_cast<int32_t>(BehaviorKind::kHitFloor));
        if (useAirDisp && hitTable->airDisplacementId > 0)
            displacementId = hitTable->airDisplacementId;

        if (displacementId > 0)
        {
            if (const auto* d = Config::getInstance()->getDisplacementConfigById(displacementId))
            {
                impulseX = d->velocity.x * towardSign;
                impulseZ = d->velocity.z * kZRate;
            }
        }

        if (impulseZ > 0.0f || useAirDisp)
            runtimeHitType = HitType::kHitLaunch;
        else if (tableHitType >= 2)
            runtimeHitType = HitType::kHitLaunch;
        else if (tableHitType == 1)
            runtimeHitType = HitType::kHitHeavy;
        else
            runtimeHitType = HitType::kHitLight;
    }

    const float oldHp = attrB->hp;
    attrB->hp         = (std::max)(0.0f, attrB->hp - dmg.damage);
    if (attrB->hp > 0.0f && attrB->hp != oldHp)
    {
        if (auto* buffMgr = BuffManager::of(defender))
            buffMgr->trigger(defender, BFEvent::HpChange, attacker, skillHitLookupId,
                             attrB->hp - oldHp);
    }

    if (dmg.damage > 0.0f && attrB->epMax > 0.0f)
    {
        auto* defMgr = SkillManager::of(defender);
        if (!defMgr || !defMgr->crazyActive)
        {
            const float maxHp = (std::max)(1.0f, attrB->basic.hpMax);
            const float gain  = (std::min)(100.0f * dmg.damage / maxHp, 100.0f) * kCrazyFromHurtValue;
            attrB->ep         = (std::min)(attrB->epMax, attrB->ep + gain);
        }
    }

    const bool superArmor = DamageCalculator::isSuperArmor(defender);
    if (!superArmor)
    {
        if (displacementId > 0)
        {
            if (auto* disp = MG_GET_COMPONENT(defender, DisplacementComponent))
            {
                if (auto* physics = MG_GET_COMPONENT(defender, PhysicsComponent))
                    disp->restoreGravity(physics);
                if (auto* d = Config::getInstance()->getDisplacementConfigById(displacementId))
                {
                    disp->start(d, towardSign);
                    disp->velocity.z *= kZRate;
                    if (auto* physics = MG_GET_COMPONENT(defender, PhysicsComponent))
                        disp->writePhysicsVelocity(physics, towardSign);
                }
            }
        }

        applyFreeze(defender, freezeMs, freezeDelay);
        if (freezeRole == 0)
            applyFreeze(attacker, freezeMs, 0);
        if (effectEntity && freezeFx == 0)
            applyFreeze(effectEntity, freezeMs, 0);

        if (auto* hitReact = MG_GET_COMPONENT(defender, HitReactComponent))
        {
            PendingHitInfo hitInfo;
            hitInfo.attackerId          = attacker->getId();
            hitInfo.hitType             = runtimeHitType;
            hitInfo.tableHitType        = tableHitType;
            hitInfo.hitId               = hitId;
            hitInfo.displacementId      = displacementId;
            hitInfo.hitState            = "Stun";
            hitInfo.hitstunMs           = hitstun;
            hitInfo.impulseX            = impulseX;
            hitInfo.impulseZ            = impulseZ;
            hitInfo.knockbackFacing     = towardSign;
            hitInfo.damage              = dmg.damage;
            hitInfo.isCrit              = dmg.isCrit;
            hitInfo.isDodge             = false;
            hitInfo.hitMust             = hitMust;
            hitInfo.hurtType            = hitTable ? hitTable->hurtType : 0;
            hitInfo.hitRigidity         = hitRigidity;
            hitInfo.freezeTimeMs        = freezeMs;
            hitInfo.freezeDelayMs       = freezeDelay;
            hitInfo.freezeControlRole   = freezeRole;
            hitInfo.freezeControlEffect = freezeFx;
            hitInfo.effectEntityId      = effectEntity ? effectEntity->getId() : INVALID_ENTITY_ID;
            hitReact->pendingHits.emplace_back(hitInfo);
        }
    }

    if (auto* buffMgr = BuffManager::of(attacker))
        buffMgr->trigger(attacker, BFEvent::AfterHit, defender, skillHitLookupId, dmg.damage);
    if (auto* buffMgr = BuffManager::of(defender))
        buffMgr->trigger(defender, BFEvent::AfterToBeHit, attacker, skillHitLookupId, dmg.damage);

    applyEffectContactBuffs(effectEntity, defender, true, false);

    if (effectEntity)
        EffectLifeSystem::spawnHitEffects(effectEntity, defender);
}

void applyEffectContactBuffs(Entity* effectEntity, Entity* target, bool hostile, bool dodged)
{
    if (!effectEntity || !target)
        return;
    auto* fx = Effect::of(effectEntity);
    if (!fx || fx->effectId <= 0)
        return;
    const auto* cfg = Config::getInstance()->getEffectConfigById(fx->effectId);
    if (!cfg)
        return;

    if (hostile)
    {
        if (dodged)
            return;
        BuffRuleUtil::addBuffIds(target, cfg->debuffId, fx->baseSkillId);
        return;
    }

    auto* ecs     = effectEntity->getECSManager();
    Entity* owner = (ecs && fx->ownerId != INVALID_ENTITY_ID) ? ecs->getEntity(fx->ownerId) : nullptr;
    if (owner && target == owner)
        BuffRuleUtil::addBuffIds(target, cfg->buffId, fx->baseSkillId);
    else if (cfg->buffAllId > 0)
    {
        if (auto* mgr = BuffManager::of(target))
            mgr->addBuff(target, cfg->buffAllId, fx->baseSkillId);
    }
}

}  // namespace CombatHit

NS_MG_END
