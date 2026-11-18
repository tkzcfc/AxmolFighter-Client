#include "mugen/sim/combat/CombatRules.h"
#include "mugen/sim/World.h"
#include "mugen/sim/ai/AiRules.h"
#include "mugen/sim/base/Rng.h"
#include "mugen/sim/combat/DamageRules.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/BuffDef.h"
#include <algorithm>
#include <utility>

namespace mugen::sim::combat::rules
{
namespace
{
// 标准伤害：英雄用物攻 + 等级标准，其他单位用基础伤害 + 十级分段标准。
Fixed standardHurt(World& world, Entity attacker)
{
    const auto& identity = world.ecs().get<IdentityComponent>(attacker);
    const auto& stats    = world.ecs().get<StatsComponent>(attacker);
    const bool hero      = identity.roleType == 1;
    const int32_t level  = hero ? identity.level : std::max(1, identity.level / 10);
    const auto* row      = conf::Config::instance().find<conf::SkillHurtConfig>(level);
    if (!row)
        return {};
    return stats.value(hero ? 5 : 15) + Fixed::fromInt(hero ? row->hurt : row->monsterHurt);
}
// 发出顿帧事实。毫秒到帧的换算留给 DebugSinkSystem。
void emitHitStop(World& world, Entity entity, Fixed freezeMs)
{
    HitStopEvent event;
    event.entity   = entity;
    event.freezeMs = freezeMs;
    world.ecs().singleton<EventQueue<HitStopEvent>>().push(event);
}
// 根据连击数返回公式固定参数 (M, N)。
std::pair<Fixed, Fixed> formulaMN(int32_t comboNum)
{
    constexpr size_t count = conf::kComboFormulaCombo.size();
    for (size_t i = 0; i < count; ++i)
    {
        const int32_t threshold = conf::kComboFormulaCombo[i];
        if (threshold > comboNum)
            return {conf::kComboFormulaM[i], conf::kComboFormulaN[i]};
        if (threshold < comboNum && i == count - 1)
            return {conf::kComboFormulaM[i], conf::kComboFormulaN[i]};
    }
    return {"0"_fx, "0"_fx};
}
// 按当前经验重算评价等级：第一个大于经验的阶段阈值决定等级；封顶经验取最高等级。
void recomputeComboGrade(ComboMeterComponent& meter)
{
    for (size_t i = 0; i < conf::kComboStageExp.size(); ++i)
    {
        if (conf::kComboStageExp[i] > meter.exp)
        {
            meter.grade = static_cast<int32_t>(i + 1);
            return;
        }
    }
    meter.grade = static_cast<int32_t>(conf::kComboStageExp.size());
}
// 将本次命中经验累加到最终主人的连击计。
void addComboFromHit(World& world, const HitContext& ctx)
{
    auto& ecs      = world.ecs();
    Entity owner   = ctx.attacker;
    if (const auto* summon = ecs.tryGet<SummonComponent>(ctx.attacker); summon && ecs.alive(summon->root))
        owner = summon->root;
    auto* meter = ecs.tryGet<ComboMeterComponent>(owner);
    if (!meter || !ctx.effectRow)
        return;
    const Fixed paramExp =
        ecs.has<StatsComponent>(owner)
            ? ecs.get<StatsComponent>(owner).finalStat(static_cast<int32_t>(conf::battle::BuffStat::ComboExpScale))
            : Fixed::fromInt(1);
    const Fixed rawExp = ctx.effectRow->comboExp * paramExp;
    ++meter->count;
    meter->maxCount  = std::max(meter->maxCount, meter->count);
    meter->timerMs   = {};
    const auto [m, n] = formulaMN(meter->count);
    Fixed comboExp    = (rawExp + m) * n;
    Fixed specialExp;
    const bool alreadyKilled =
        std::find(meter->killedTargets.begin(), meter->killedTargets.end(), ctx.target) != meter->killedTargets.end();
    if (health::rules::isDead(ecs.get<HealthComponent>(ctx.target)) && !alreadyKilled)
    {
        specialExp = conf::kComboKillExp;
        meter->killedTargets.push_back(ctx.target);
    }
    else if (ecs.has<StatusFlagsComponent>(ctx.target) &&
             ecs.get<StatusFlagsComponent>(ctx.target).active(mugen::ExtraState::Stun))
        specialExp = conf::kComboStunExp;
    Fixed typeRate = "1"_fx;
    if (ecs.has<IdentityComponent>(ctx.target) && ecs.get<IdentityComponent>(ctx.target).roleType == 4)
        typeRate = conf::kComboBossExpRate;
    comboExp = (comboExp + specialExp) * typeRate + meter->exp;
    if (comboExp > conf::kComboStageExp.back())
        comboExp = conf::kComboStageExp.back();
    meter->exp = comboExp;
    recomputeComboGrade(*meter);
    meter->maxGrade = std::max(meter->maxGrade, meter->grade);
}
// 首次命中记录当前生命减去最大生命比例后的阈值。
void initHitProtect(World& world, Entity target, HitStateComponent& hitState)
{
    if (!world.setup().pvp || hitState.hitProtectHp)
        return;
    const auto& health          = world.ecs().get<HealthComponent>(target);
    hitState.hitProtectHp       = std::max(Fixed{}, health.hp - health.maxHp * conf::kHitProtectHpScale);
    hitState.hitProtectTime     = Fixed::fromInt(conf::kHitProtectCleanTime);
    hitState.hitProtectExecuted = false;
}
}  // namespace

// 步骤 1：未销毁、主人存在、阵营敌对、间隔/次数/去重、未死且可受击。
bool gate(World& world, const ContactEvent& contact, HitContext& ctx)
{
    const Entity effect = contact.a;
    const Entity target = contact.b;
    auto& ecs           = world.ecs();
    if (!ecs.alive(effect) || ecs.has<DespawningComponent>(effect) || !ecs.has<EffectInfoComponent>(effect))
        return false;
    if (!ecs.alive(target) || ecs.has<DespawningComponent>(target) || !ecs.has<HealthComponent>(target))
        return false;
    const auto& effectInfo = ecs.get<EffectInfoComponent>(effect);
    const auto* effectRow  = conf::Config::instance().find<conf::battle::EffectDef>(effectInfo.configId);
    if (!effectRow || effectRow->hitId <= 0)
        return false;
    const auto* hitRow = conf::Config::instance().find<conf::battle::HitDef>(effectRow->hitId);
    if (!hitRow)
        return false;
    if (!ecs.alive(effectInfo.owner) || !ecs.has<StatsComponent>(effectInfo.owner))
        return false;
    const auto& ownerIdentity  = ecs.get<IdentityComponent>(effectInfo.owner);
    const auto& targetIdentity = ecs.get<IdentityComponent>(target);
    if ((static_cast<uint32_t>(ownerIdentity.hostileMask) & static_cast<uint32_t>(targetIdentity.camp)) !=
        static_cast<uint32_t>(targetIdentity.camp))
        return false;

    const auto& tracker      = ecs.get<HitTrackerComponent>(effect);
    const auto& targetHealth = ecs.get<HealthComponent>(target);
    if (tracker.hitInterval > Fixed{} || (hitRow->hitCounts != -1 && tracker.hitCount >= hitRow->hitCounts))
        return false;
    if (targetHealth.isDead() || !targetHealth.beHitEnabled)
        return false;
    if (hitRow->hitInterval == -1)
    {
        if (std::find(tracker.hitTargets.begin(), tracker.hitTargets.end(), target) != tracker.hitTargets.end())
            return false;
        if (hitRow->hitCounts != -1 && static_cast<int32_t>(tracker.hitTargets.size()) >= hitRow->hitCounts)
            return false;
    }

    // 填充上下文（hitType == -1 也要填好，后续按 hitType 走无伤害路径）。
    ctx.contact     = contact;
    ctx.effect      = effect;
    ctx.target      = target;
    ctx.attacker    = effectInfo.owner;
    ctx.effectId    = effectInfo.configId;
    ctx.skillId     = effectInfo.skillId;
    ctx.baseSkillId = effectInfo.baseSkillId;
    ctx.slot        = effectInfo.slot;
    ctx.hit         = hitRow;
    ctx.effectRow   = effectRow;

    // 受击规则（原版 dealWithBeHitBefore）：起身保护 / 死亡起身醒来无敌 / 不打倒地浮空。
    const auto& roleState = ecs.get<RoleStateComponent>(target);
    if (effectRow->hitExtraControl.size() > 1 && effectRow->hitExtraControl[1] == 0 &&
        roleState.state == RoleStateId::GetUp && hitRow->hitMust != 1)
        return false;
    // hitType == -1（无伤害也无硬直）：仍算命中，且不受死亡/起身/醒来无敌拦截（原版 dealWithBeHitBefore 先于状态拦截返回）。
    if (hitRow->hitType == -1)
        return true;
    if (roleState.state == RoleStateId::Death || roleState.state == RoleStateId::GetUp ||
        roleState.state == RoleStateId::Wake)
        return false;
    const bool floor = roleState.state == RoleStateId::HitFloor &&
                       ecs.get<TransformComponent>(target).position.y == Fixed{};
    const bool air = roleState.state == RoleStateId::HitUp || roleState.state == RoleStateId::HitDown;
    if ((hitRow->hitCondition == 0 || hitRow->hitCondition == 2) && floor)
        return false;
    if ((hitRow->hitCondition == 1 || hitRow->hitCondition == 2) && air)
        return false;
    return true;
}

// 步骤 2：记一次命中。hitInterval 为 -1 时按目标去重。
// 命中间隔不在本步写入：旧实现把间隔推迟到帧末（下一帧初）才设置，同帧多个目标都能命中。
void recordHit(World& world, const HitContext& ctx)
{
    auto& tracker = world.ecs().get<HitTrackerComponent>(ctx.effect);
    if (ctx.hit->hitInterval == -1)
        tracker.hitTargets.push_back(ctx.target);
    ++tracker.hitCount;
}

// 帧末提交命中间隔：本帧发生过命中的特效从下一帧起进入间隔冷却。
void commitHitInterval(World& world, Entity effect)
{
    auto& tracker = world.ecs().get<HitTrackerComponent>(effect);
    if (world.ecs().has<EffectInfoComponent>(effect))
    {
        const auto& info = world.ecs().get<EffectInfoComponent>(effect);
        const auto* effectRow = conf::Config::instance().find<conf::battle::EffectDef>(info.configId);
        const auto* hitRow = effectRow && effectRow->hitId > 0
                                 ? conf::Config::instance().find<conf::battle::HitDef>(effectRow->hitId)
                                 : nullptr;
        if (hitRow && hitRow->hitInterval != -1)
            tracker.hitInterval = Fixed::fromInt(hitRow->hitInterval);
    }
}

// 步骤 5：非必中才抽样 Dodge。成功发出 Miss。
bool resolveDodge(World& world, const HitContext& ctx)
{
    if (ctx.hit->hitMust != 0)
        return false;
    DamageInput input;
    input.attacker = &world.ecs().get<StatsComponent>(ctx.attacker);
    input.target   = &world.ecs().get<StatsComponent>(ctx.target);
    input.hit      = ctx.hit;
    if (!attributeDodge(input, rng::draw(world, rng::DrawKind::Dodge)))
        return false;
    HitEvent event;
    event.effect   = ctx.effect;
    event.target   = ctx.target;
    event.hitDefId = ctx.hit->id;
    event.effectId = ctx.effectId;
    event.result   = HitResult::Miss;
    world.ecs().singleton<EventQueue<HitEvent>>().push(event);
    return true;
}

// 步骤 8：先暴击再浮动。
void computeDamage(World& world, HitContext& ctx)
{
    DamageInput input;
    input.attacker     = &world.ecs().get<StatsComponent>(ctx.attacker);
    input.target       = &world.ecs().get<StatsComponent>(ctx.target);
    input.hit          = ctx.hit;
    input.standardHurt = standardHurt(world, ctx.attacker);
    input.pvp          = world.setup().pvp;
    const Fixed critDraw  = rng::draw(world, rng::DrawKind::Crit);
    const Fixed floatDraw = rng::draw(world, rng::DrawKind::DamageFloat);
    ctx.damage          = calculateDamage(input, critDraw, floatDraw, ctx.critical);
}

// 步骤 9：扣血即时生效。S2: trigger BeforeDeath（护盾、锁血、免死在 health::rules 写入前）。
void applyDamage(World& world, const HitContext& ctx)
{
    health::rules::applyDelta(world, ctx.target, -ctx.damage, HpCause::Hit, ctx.attacker, ctx.effect);
}

// 步骤 10：特效与攻击者覆盖写 PauseComponent（R18），受击者经 ScheduledActions 延迟写入（R13）。
void applyHitStop(World& world, const HitContext& ctx)
{
    const Fixed freeze = Fixed::fromInt(ctx.hit->freezeTime);
    auto& ecs          = world.ecs();
    if (ctx.hit->freezeTimeControlEffect == 0)
    {
        ecs.get<PauseComponent>(ctx.effect).freezeMs = freeze;
        emitHitStop(world, ctx.effect, freeze);
    }
    if (ctx.hit->freezeTimeControlRole == 0)
    {
        ecs.get<PauseComponent>(ctx.attacker).freezeMs = freeze;
        emitHitStop(world, ctx.attacker, freeze);
    }
    if (ecs.has<PauseComponent>(ctx.target))
    {
        auto& actions = ecs.singleton<ScheduledActions>();
        actions.cancelOwner(ctx.target);
        actions.add(ctx.target, Fixed::fromInt(ctx.hit->freezeTimeDelay), freeze);
    }
}

// 步骤 10：写受击请求。状态转移留给下一帧 FSM。死亡与霸体不进本函数。
void requestHitReaction(World& world, const HitContext& ctx)
{
    auto& ecs              = world.ecs();
    const auto& roleState  = ecs.get<RoleStateComponent>(ctx.target);
    const bool inHitFamily = role::isHitState(roleState.state);
    const auto& targetIdentity = ecs.get<IdentityComponent>(ctx.target);
    const bool dead             = health::rules::isDead(ecs.get<HealthComponent>(ctx.target));
    const auto* roleRow =
        conf::Config::instance().find<conf::battle::RoleDef>(targetIdentity.configId);
    // 受击族内再次被击中时，旧实现只对非英雄/非 Boss 或未死目标排队 hit_id；英雄和 Boss 已死时不排队，
    // 因此不重启位移（对应 _hitReaction 的 isTobeHit 分支）。
    const bool heroOrBoss = targetIdentity.roleType == 1 || targetIdentity.roleType == 4;
    const bool skipHitQueue = inHitFamily && dead && heroOrBoss;

    auto& hitState              = ecs.get<HitStateComponent>(ctx.target);
    const auto& effectTransform = ecs.get<TransformComponent>(ctx.effect);
    const int32_t facing        = -effectTransform.towardX;
    ecs.get<TransformComponent>(ctx.target).towardX = facing;
    ecs.get<TransformComponent>(ctx.target).vectorX = Fixed::fromInt(facing);
    // 硬直 = stiff_time × (1 + AddFloorTime)；倒地中不重置硬直，角色覆盖 hitStiffTime 时用它。
    const Fixed floorScale = Fixed::fromInt(1) + ecs.get<StatsComponent>(ctx.target).finalStat(115);
    if (hitState.stiffTime == Fixed{} || roleState.state != RoleStateId::HitFloor)
        hitState.stiffTime = Fixed::fromInt(ctx.hit->stiffTime) * floorScale;
    if (roleRow && roleRow->hitStiffTime != -1)
        hitState.stiffTime = Fixed::fromInt(roleRow->hitStiffTime) * floorScale;
    int32_t displacementId = -1;
    if (roleRow && roleRow->hitDisplacementId != 0)
        displacementId = (roleRow->hitDisplacementId == -1 || ctx.hit->hitType == 0) ? ctx.hit->displacementId
                                                                                        : roleRow->hitDisplacementId;
    if (!skipHitQueue)
    {
        hitState.pendingHitDefId       = ctx.hit->id;
        hitState.pendingHitType        = ctx.hit->hitType;
        hitState.pendingDisplacementId = displacementId;
        hitState.pendingTowardX        = facing;
        // 浮空值已耗尽时，每次受击受击计数 +1（追击连段越长硬直越短、下落越快）。
        if (inHitFamily && hitState.rigidity <= Fixed{} && roleRow && roleRow->rigidity > 0)
            ++hitState.hitCounts;
    }
    if (inHitFamily)
    {
        if (hitState.hitProtectHp && !hitState.hitProtectExecuted)
            hitState.hitProtectTime = Fixed::fromInt(conf::kHitProtectCleanTime);
    }
    else if (!dead)
        initHitProtect(world, ctx.target, hitState);
    if (!inHitFamily && !dead && ctx.hit->hitType != -1)
        skill::rules::forceInterrupt(world, ctx.target);
}

// 步骤 14：同目标已有同 id 时由 spawnHitEffect 复用。
void spawnHitEffects(World& world, const HitContext& ctx)
{
    for (const int32_t id : ctx.effectRow->hitEffectIds)
        if (id > 0)
            effect::rules::spawnHitEffect(world, ctx.attacker, ctx.effect, id, ctx.target,
                                          {ctx.skillId, ctx.baseSkillId, ctx.slot, 0, 0},
                                          ctx.contact);
}

// 步骤 15：命中结果。音效 Cue 留给 S2。
void emitHit(World& world, const HitContext& ctx)
{
    const auto& targetHealth = world.ecs().get<HealthComponent>(ctx.target);
    HitEvent hitEvent;
    hitEvent.attacker = ctx.attacker;
    hitEvent.effect   = ctx.effect;
    hitEvent.target   = ctx.target;
    hitEvent.hitDefId = ctx.hit->id;
    hitEvent.effectId = ctx.effectId;
    hitEvent.result   = HitResult::Hit;
    hitEvent.damage   = ctx.damage;
    hitEvent.critical = ctx.critical;
    hitEvent.killed   = targetHealth.hp <= Fixed{};
    hitEvent.contact  = ctx.contact.point;
    world.ecs().singleton<EventQueue<HitEvent>>().push(hitEvent);
}

// 命中后按倍率回复 EP；曝气状态下不回复。
void recoverEp(World& world, Entity entity, Fixed value)
{
    auto& ecs = world.ecs();
    if (!ecs.has<HealthComponent>(entity))
        return;
    if (const auto* cast = ecs.tryGet<SkillCastComponent>(entity); cast && cast->crazy)
        return;
    const Fixed scale =
        ecs.has<StatsComponent>(entity)
            ? ecs.get<StatsComponent>(entity).finalStat(static_cast<int32_t>(conf::battle::BuffStat::EpRecoveryScale))
            : Fixed::fromInt(1);
    auto& health = ecs.get<HealthComponent>(entity);
    health.ep    = std::clamp(health.ep + value * scale, Fixed{}, health.maxEp);
}

// 命中结算：仇恨、连击评价与受击回 EP（在 AfterToBeHit 之前）。
void settleHitFeedback(World& world, const HitContext& ctx)
{
    auto& ecs = world.ecs();
    ai::rules::addThreat(world, ctx.target, ctx.attacker, ctx.damage);
    addComboFromHit(world, ctx);
    if (!ecs.has<HealthComponent>(ctx.target))
        return;
    const Fixed maxHp = ecs.get<HealthComponent>(ctx.target).maxHp;
    if (maxHp > Fixed{})
        recoverEp(world, ctx.target,
                  std::min("100"_fx * ctx.damage / maxHp, "100"_fx) * conf::kCrazyFromHurtValue);
}

// autoRelease 2/3：命中目标数达标则回收。2 关闭后继，3 保留后继给 DespawnSystem。
void expireByHitCount(World& world)
{
    world.ecs().each<LifetimeComponent, EffectInfoComponent, HitTrackerComponent>(
        [&](Entity entity, const LifetimeComponent& life, EffectInfoComponent& info, const HitTrackerComponent& tracker) {
            if (world.ecs().has<DespawningComponent>(entity))
                return;
            if (life.autoRelease != 2 && life.autoRelease != 3)
                return;
            const auto* effectRow = conf::Config::instance().find<conf::battle::EffectDef>(info.configId);
            const auto* hitRow    = effectRow && effectRow->hitId > 0
                                        ? conf::Config::instance().find<conf::battle::HitDef>(effectRow->hitId)
                                        : nullptr;
            if (!hitRow || hitRow->hitInterval != -1 || hitRow->hitCounts == -1)
                return;
            if (static_cast<int32_t>(tracker.hitTargets.size()) < hitRow->hitCounts)
                return;
            info.next = life.autoRelease == 3;
            life::rules::despawn(world, entity, DespawnReason::HitCount);
        });
}
}  // namespace mugen::sim::combat::rules
