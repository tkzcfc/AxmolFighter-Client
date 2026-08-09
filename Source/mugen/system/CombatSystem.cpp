#include "CombatSystem.h"

#include "mugen/GameWord.h"
#include "mugen/Components.h"
#include "mugen/combat/DamageCalculator.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Entity.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

NS_MG_BEGIN

namespace
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
        return true;  // 缺标识默认可打

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

/** hitTarget: -1 Both, 0 Enemy, 1 Friend */
bool passHitTarget(int32_t hitTarget, bool hostile)
{
    if (hitTarget == 0)
        return hostile;
    if (hitTarget == 1)
        return !hostile;
    return true;
}

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
                     (behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitUp));
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

bool isAirborneDefender(Entity* defender)
{
    if (!defender)
        return false;
    auto* behavior = MG_GET_COMPONENT(defender, BehaviorComponent);
    auto* physics  = MG_GET_COMPONENT(defender, PhysicsComponent);
    if (behavior && (behavior->statusTags & StateTag::kTagAirborne))
        return true;
    if (behavior && behavior->currentKind == static_cast<int32_t>(BehaviorKind::kHitUp))
        return true;
    if (physics && !physics->onGround)
        return true;
    return false;
}

void resetMeleeHits(SkillCastComponent* cast, int32_t skillId)
{
    if (!cast)
        return;
    if (skillId <= 0)
    {
        cast->meleeHitSkillId = 0;
        cast->meleeHitTargetIds.clear();
        cast->meleeHitCounts.clear();
        cast->meleeHitCooldowns.clear();
        return;
    }
    if (cast->meleeHitSkillId != skillId)
    {
        cast->meleeHitSkillId = skillId;
        cast->meleeHitTargetIds.clear();
        cast->meleeHitCounts.clear();
        cast->meleeHitCooldowns.clear();
    }
}

int findMeleeHitIndex(SkillCastComponent* cast, EntityId targetId)
{
    if (!cast)
        return -1;
    for (size_t i = 0; i < cast->meleeHitTargetIds.size(); ++i)
    {
        if (cast->meleeHitTargetIds[i] == static_cast<uint32_t>(targetId))
            return static_cast<int>(i);
    }
    return -1;
}

void queueHit(Entity* attacker,
             Entity* defender,
             Entity* effectEntity,
             const SkillHitTableConfig* hitTable,
             int32_t skillHitLookupId,
             Random& rng)
{
    if (!attacker || !defender)
        return;

    if (auto* attrDead = MG_GET_COMPONENT(defender, AttributeComponent))
    {
        if (attrDead->currentAttribute.hp <= 0.0f)
            return;
    }

    if (DamageCalculator::isInvincible(defender))
        return;

    if (hitTable && !passHitCondition(defender, hitTable->hitCondition))
        return;

    auto* attrA = MG_GET_COMPONENT(attacker, AttributeComponent);
    auto* attrB = MG_GET_COMPONENT(defender, AttributeComponent);
    if (!attrB)
        return;

    const bool hitMust = hitTable && hitTable->hitMust == 1;
    const int32_t defLv = resolveLevel(defender);
    const int32_t atkLv = resolveLevel(attacker);
    const bool heroDef  = isHeroEntity(defender);
    const bool heroAtk  = isHeroEntity(attacker);
    const auto* hurtStd =
        Config::getInstance()->getSkillHurtConfigById(DamageCalculator::hurtStandardId(defLv, heroDef));
    const auto* atkHurtStd = heroAtk ? Config::getInstance()->getSkillHurtConfigById(
                                           DamageCalculator::hurtStandardId(atkLv, true))
                                     : nullptr;

    DamageInput din;
    din.attacker         = attrA;
    din.defender         = attrB;
    din.hitCfg           = hitTable;
    din.hurtStd          = hurtStd;
    din.attackerHurtStd  = atkHurtStd;
    din.skillAddition    = 0.0f;
    din.hitMust          = hitMust;
    din.isHeroDefender   = heroDef;

    // 无 hit 表时用默认轻击倍率
    SkillHitTableConfig fallbackHit;
    if (!hitTable)
    {
        fallbackHit.hurtRate = 1.0f;
        fallbackHit.hurtType = 0;
        fallbackHit.hitType  = static_cast<int32_t>(HitType::kHitLight);
        fallbackHit.stiffTime = 250;
        din.hitCfg           = &fallbackHit;
    }

    DamageResult dmg = DamageCalculator::calculate(din, rng);
    if (dmg.isDodge)
        return;

    int32_t hitstun = 250;
    int32_t hitType = static_cast<int32_t>(HitType::kHitLight);
    int32_t displacementId = -1;
    float impulseX = 0.0f;
    float impulseZ = 0.0f;
    int32_t freezeMs = 0;
    int32_t freezeDelay = 0;
    int32_t freezeRole = 0;
    int32_t freezeFx = 0;
    int32_t hitRigidity = 0;

    if (hitTable)
    {
        hitType = hitTable->hitType <= 0 ? static_cast<int32_t>(HitType::kHitLight) : hitTable->hitType;
        hitstun = hitTable->stiffTime > 0 ? hitTable->stiffTime : 0;
        if (hitstun <= 0)
            hitstun = hitTable->hitRigidity > 0 ? hitTable->hitRigidity : 250;
        hitRigidity    = hitTable->hitRigidity;
        displacementId = hitTable->displacementId;
        freezeMs       = hitTable->freezeTime;
        freezeDelay    = hitTable->freezeTimeDelay;
        freezeRole     = hitTable->freezeTimeControlRole;
        freezeFx       = hitTable->freezeTimeControlEffect;

        if (hitTable->airDisplacementId > 0 && isAirborneDefender(defender))
        {
            displacementId = hitTable->airDisplacementId;
            hitType        = static_cast<int32_t>(HitType::kHitLaunch);
        }
        else if (hitTable->floorDisplacementId > 0 &&
                 MG_GET_COMPONENT(defender, BehaviorComponent) &&
                 (MG_GET_COMPONENT(defender, BehaviorComponent)->statusTags & StateTag::kTagDownState))
        {
            displacementId = hitTable->floorDisplacementId;
        }

        if (displacementId > 0)
        {
            if (const auto* d = Config::getInstance()->getDisplacementConfigById(displacementId))
            {
                auto* atf = MG_GET_COMPONENT(attacker, TransformComponent);
                const float facing =
                    (atf && atf->facingDirection == FacingDirection::kFacingLeft) ? -1.0f : 1.0f;
                impulseX = d->velocity.x * facing;
                impulseZ = d->velocity.z;
                if (impulseZ > 0.0f && hitType < static_cast<int32_t>(HitType::kHitLaunch))
                    hitType = static_cast<int32_t>(HitType::kHitLaunch);
            }
        }
    }

    // 硬直抗性
    if (hitstun > 0)
    {
        const float resistRatio =
            std::max(0.0f, std::min(100.0f, attrB->currentAttribute.hitRecovery)) / 100.0f;
        hitstun = std::max(HITSTUN_MIN_MS, static_cast<int32_t>(hitstun * (1.0f - resistRatio)));
    }

    // 扣血（非霸体也扣；霸体只跳过硬直/顿帧）
    attrB->currentAttribute.hp = std::max(0.0f, attrB->currentAttribute.hp - dmg.damage);

    const bool superArmor = DamageCalculator::isSuperArmor(defender);
    if (!superArmor)
    {
        if (displacementId > 0)
        {
            if (auto* disp = MG_GET_COMPONENT(defender, DisplacementComponent))
                if (auto* d = Config::getInstance()->getDisplacementConfigById(displacementId))
                    disp->start(d);
        }

        // 顿帧：受击方始终（可带 delay）；攻击方/特效立即冻（黑月 delay 仅受击）
        applyFreeze(defender, freezeMs, freezeDelay);
        if (freezeRole == 0)
            applyFreeze(attacker, freezeMs, 0);
        if (effectEntity && freezeFx == 0)
            applyFreeze(effectEntity, freezeMs, 0);

        if (auto* hitReact = MG_GET_COMPONENT(defender, HitReactComponent))
        {
            PendingHitInfo hitInfo;
            hitInfo.attackerId          = attacker->getId();
            hitInfo.hitType             = static_cast<HitType>(hitType);
            hitInfo.hitState            = "Stun";
            hitInfo.hitstunMs           = hitstun;
            hitInfo.impulseX            = impulseX;
            hitInfo.impulseZ            = impulseZ;
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
            hitInfo.effectEntityId =
                effectEntity ? effectEntity->getId() : INVALID_ENTITY_ID;
            hitReact->pendingHits.emplace_back(hitInfo);
        }
    }

    (void)skillHitLookupId;
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

    // 冻结倒计时（全实体属性）
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
            continue;  // 延迟期间尚未冻结，逻辑照常；到达 0 后下帧开始冻
        }
        if (attr->freezeRemainingMs > 0)
            attr->freezeRemainingMs = std::max(0, attr->freezeRemainingMs - dtMs);
    }

    for (size_t i = 0; i < entities.size(); ++i)
    {
        auto* entityA    = entities[i];
        auto* avatarCompA = MG_GET_COMPONENT(entityA, AvatarComponent);
        auto* behaviorA  = MG_GET_COMPONENT(entityA, BehaviorComponent);
        auto* castA      = MG_GET_COMPONENT(entityA, SkillCastComponent);
        auto* attrA      = MG_GET_COMPONENT(entityA, AttributeComponent);
        if (attrA && attrA->freezeRemainingMs > 0 && attrA->freezeDelayMs <= 0)
            continue;

        const int32_t skillId = castA ? castA->activeSkillAttackId : 0;

        if (skillId <= 0)
        {
            if (castA)
                resetMeleeHits(castA, 0);
            continue;
        }

        resetMeleeHits(castA, skillId);
        // 冷却倒计时
        for (int32_t& cd : castA->meleeHitCooldowns)
            if (cd > 0)
                cd = (std::max)(0, cd - dtMs);

        std::vector<DamageBox> attackBoxes = avatarCompA->getAttackBoxes();
        if (attackBoxes.empty())
        {
            float radius = 40.0f;
            if (behaviorA && behaviorA->roleConfig && behaviorA->roleConfig->radius > 0)
                radius = behaviorA->roleConfig->radius;
            attackBoxes.push_back(makeRadiusAttackBox(MG_GET_COMPONENT(entityA, TransformComponent), radius));
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

            if (!passHitTarget(-1, isHostile(entityA, entityB)))
                continue;
            if (!isHostile(entityA, entityB))
                continue;

            int idx = findMeleeHitIndex(castA, entityB->getId());
            if (idx >= 0 && castA->meleeHitCooldowns[static_cast<size_t>(idx)] > 0)
                continue;

            const int32_t hitInterval = hitTable ? hitTable->hitInterval : 0;
            const int32_t maxHits     = hitTable && hitTable->hitCounts > 0 ? hitTable->hitCounts : 1;
            const int32_t curCount    = idx >= 0 ? castA->meleeHitCounts[static_cast<size_t>(idx)] : 0;
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
            if (!boxesOverlap(attackBoxes, avatarCompB->getDamageBoxes()))
                continue;

            if (idx < 0)
            {
                castA->meleeHitTargetIds.push_back(static_cast<uint32_t>(entityB->getId()));
                castA->meleeHitCounts.push_back(1);
                castA->meleeHitCooldowns.push_back(hitInterval < 0 ? 0 : (hitInterval > 0 ? hitInterval : 0));
            }
            else
            {
                ++castA->meleeHitCounts[static_cast<size_t>(idx)];
                castA->meleeHitCooldowns[static_cast<size_t>(idx)] =
                    hitInterval < 0 ? 0 : (hitInterval > 0 ? hitInterval : 0);
            }

            queueHit(entityA, entityB, nullptr, hitTable, skillId, rng);
        }
    }

    // 特效攻击
    Signature effectSig;
    effectSig.set(ecs->getComponentTypeId("EffectLifetimeComponent"));
    auto effectEntities = ecs->getEntitiesBySignature(effectSig);
    for (Entity* effectEntity : effectEntities)
    {
        auto* fx = MG_GET_COMPONENT(effectEntity, EffectLifetimeComponent);
        auto* tf = MG_GET_COMPONENT(effectEntity, TransformComponent);
        auto* fxAttr = MG_GET_COMPONENT(effectEntity, AttributeComponent);
        if (!fx || !tf || fx->skillHitId <= 0)
            continue;
        if (fxAttr && fxAttr->freezeRemainingMs > 0 && fxAttr->freezeDelayMs <= 0)
            continue;
        if (fx->hitCooldownMs > 0)
        {
            fx->hitCooldownMs = (std::max)(0, fx->hitCooldownMs - dtMs);
            continue;
        }

        Entity* owner = fx->ownerId != INVALID_ENTITY_ID ? ecs->getEntity(fx->ownerId) : effectEntity;
        if (!owner)
            owner = effectEntity;

        const auto* hitTable = Config::getInstance()->getSkillHitTableConfigById(fx->skillHitId);
        const auto* effectCfg =
            fx->effectId > 0 ? Config::getInstance()->getEffectConfigById(fx->effectId) : nullptr;
        const int32_t hitTarget = effectCfg ? effectCfg->hitTarget : 0;  // 默认敌方

        std::vector<DamageBox> attackBoxes{makeRadiusAttackBox(tf, fx->radius)};
        const int32_t maxHits     = hitTable && hitTable->hitCounts > 0 ? hitTable->hitCounts : 1;
        const int32_t hitInterval = hitTable ? hitTable->hitInterval : 80;

        if (hitInterval >= 0 && fx->hitCount >= maxHits)
            continue;

        for (Entity* defender : entities)
        {
            if (defender == owner || defender == effectEntity)
                continue;

            const bool hostile = isHostile(owner, defender);
            if (!passHitTarget(hitTarget, hostile))
                continue;

            auto* avatarB = MG_GET_COMPONENT(defender, AvatarComponent);
            if (!avatarB || avatarB->getDamageBoxes().empty())
                continue;

            if (hitInterval < 0)
            {
                bool seen = false;
                for (uint32_t id : fx->hitEntityIds)
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

            if (!boxesOverlap(attackBoxes, avatarB->getDamageBoxes()))
                continue;

            ++fx->hitCount;
            if (hitInterval < 0)
                fx->hitEntityIds.push_back(defender->getId());
            else
                fx->hitCooldownMs = hitInterval > 0 ? hitInterval : 80;

            queueHit(owner, defender, effectEntity, hitTable, fx->skillHitId, rng);

            if (hitInterval >= 0 && fx->hitCount >= maxHits)
                break;
        }
    }
}

NS_MG_END
