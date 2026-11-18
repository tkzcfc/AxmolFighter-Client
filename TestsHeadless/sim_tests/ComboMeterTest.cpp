#include "doctest.h"
#include "BuffTestSupport.h"
#include "mugen/sim/World.h"
#include "mugen/sim/Systems.h"
#include "mugen/sim/combat/CombatRules.h"
#include "mugen/sim/ai/AiComponents.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/conf/battle/BuffDef.h"
#include "mugen/conf/battle/EffectDef.h"
#include "mugen/conf/battle/HitDef.h"

#include <utility>

using namespace mugen;
using namespace mugen::sim;

namespace
{
// 构造空房间战斗世界（关 AI）。
World makeWorld()
{
    buff_test::loadAssets();
    World world;
    WorldSetup setup;
    setup.seed      = 31;
    setup.mode      = WorldMode::Battle;
    setup.aiEnabled = false;
    world.init(setup);
    return world;
}

// 组装一次命中结算上下文。
combat::rules::HitContext makeCtx(Entity attacker, Entity target, const conf::battle::EffectDef* row, Fixed damage)
{
    combat::rules::HitContext ctx;
    ctx.attacker  = attacker;
    ctx.target    = target;
    ctx.effectRow = row;
    ctx.damage    = damage;
    return ctx;
}

// 根据连击数返回公式固定参数 (M, N)，与结算实现一致。
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
}  // namespace

TEST_CASE("combo meter: hit increments count, exp, grade and max stats")
{
    World world           = makeWorld();
    const Entity attacker = buff_test::spawnActor(world, 1, 2, 4);
    const Entity target   = buff_test::spawnActor(world, 2, 4, 2);

    conf::battle::EffectDef row;
    row.comboExp = "10"_fx;
    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &row, Fixed{}));

    const auto& meter = world.ecs().get<ComboMeterComponent>(attacker);
    CHECK(meter.count == 1);
    CHECK(meter.maxCount == 1);
    const auto [m, n] = formulaMN(1);
    const Fixed base1 = (row.comboExp + m) * n;
    CHECK(meter.exp == base1);
    CHECK(meter.grade == 1);
    CHECK(meter.maxGrade == 1);
    CHECK(meter.timerMs == Fixed{});

    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &row, Fixed{}));
    CHECK(meter.count == 2);
    CHECK(meter.maxCount == 2);
    CHECK(meter.grade == 1);
    CHECK(meter.maxGrade == 1);

    conf::battle::EffectDef hugeRow;
    hugeRow.comboExp = "3000"_fx;
    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &hugeRow, Fixed{}));
    CHECK(meter.exp == conf::kComboStageExp.back());
    CHECK(meter.grade == static_cast<int32_t>(conf::kComboStageExp.size()));
    CHECK(meter.maxGrade == static_cast<int32_t>(conf::kComboStageExp.size()));
}

TEST_CASE("combo meter: summon hit credits root owner")
{
    World world           = makeWorld();
    const Entity owner    = buff_test::spawnActor(world, 1, 2, 4);
    const Entity summon   = buff_test::spawnActor(world, 3, 2, 4);
    const Entity target   = buff_test::spawnActor(world, 2, 4, 2);
    SummonComponent link;
    link.owner        = owner;
    link.root         = owner;
    link.remainingMs  = "-1"_fx;
    world.ecs().add(summon, link);

    conf::battle::EffectDef row;
    row.comboExp = "10"_fx;
    combat::rules::settleHitFeedback(world, makeCtx(summon, target, &row, Fixed{}));

    const auto& ownerMeter  = world.ecs().get<ComboMeterComponent>(owner);
    const auto& summonMeter = world.ecs().get<ComboMeterComponent>(summon);
    CHECK(ownerMeter.count == 1);
    CHECK(ownerMeter.maxCount == 1);
    CHECK(ownerMeter.exp == "10"_fx * "1.2"_fx);
    CHECK(ownerMeter.grade == 1);
    CHECK(summonMeter.count == 0);
    CHECK(summonMeter.exp == Fixed{});
}

TEST_CASE("combo meter: kill bonus once per target, stun bonus, boss rate")
{
    World world           = makeWorld();
    const Entity attacker = buff_test::spawnActor(world, 1, 2, 4);
    const Entity target   = buff_test::spawnActor(world, 2, 4, 2);
    const Entity other    = buff_test::spawnActor(world, 2, 4, 2);

    conf::battle::EffectDef row;
    row.comboExp      = "10"_fx;
    const Fixed base1 = "10"_fx * "1.2"_fx;

    auto& health = world.ecs().get<HealthComponent>(target);
    health.hp    = {};
    health.dead  = true;
    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &row, Fixed{}));
    auto& meter = world.ecs().get<ComboMeterComponent>(attacker);
    CHECK(meter.exp == base1 + conf::kComboKillExp);
    CHECK(meter.killedTargets.size() == 1);
    CHECK(meter.killedTargets.front() == target);

    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &row, Fixed{}));
    CHECK(meter.killedTargets.size() == 1);
    CHECK(meter.exp == base1 + conf::kComboKillExp + base1);

    auto& otherHealth = world.ecs().get<HealthComponent>(other);
    otherHealth.hp    = {};
    otherHealth.dead  = true;
    combat::rules::settleHitFeedback(world, makeCtx(attacker, other, &row, Fixed{}));
    CHECK(meter.killedTargets.size() == 2);
    CHECK(meter.exp == base1 + conf::kComboKillExp + base1 + base1 + conf::kComboKillExp);

    World stunWorld           = makeWorld();
    const Entity stunAttacker = buff_test::spawnActor(stunWorld, 1, 2, 4);
    const Entity stunned      = buff_test::spawnActor(stunWorld, 2, 4, 2);
    stunWorld.ecs().get<StatusFlagsComponent>(stunned).add(ExtraState::Stun);
    combat::rules::settleHitFeedback(stunWorld, makeCtx(stunAttacker, stunned, &row, Fixed{}));
    CHECK(stunWorld.ecs().get<ComboMeterComponent>(stunAttacker).exp == base1 + conf::kComboStunExp);
    CHECK(stunWorld.ecs().get<ComboMeterComponent>(stunAttacker).killedTargets.empty());

    World bossWorld           = makeWorld();
    const Entity bossAttacker = buff_test::spawnActor(bossWorld, 1, 2, 4);
    const Entity boss         = buff_test::spawnActor(bossWorld, 4, 4, 2);
    combat::rules::settleHitFeedback(bossWorld, makeCtx(bossAttacker, boss, &row, Fixed{}));
    const Fixed bossHit1 = base1 * conf::kComboBossExpRate;
    CHECK(bossWorld.ecs().get<ComboMeterComponent>(bossAttacker).exp == bossHit1);

    auto& bossHealth = bossWorld.ecs().get<HealthComponent>(boss);
    bossHealth.hp    = {};
    bossHealth.dead  = true;
    combat::rules::settleHitFeedback(bossWorld, makeCtx(bossAttacker, boss, &row, Fixed{}));
    CHECK(bossWorld.ecs().get<ComboMeterComponent>(bossAttacker).exp ==
          bossHit1 + (base1 + conf::kComboKillExp) * conf::kComboBossExpRate);
}

TEST_CASE("combo meter: ComboExpScale multiplies effect comboExp")
{
    World world           = makeWorld();
    const Entity attacker = buff_test::spawnActor(world, 1, 2, 4);
    const Entity target   = buff_test::spawnActor(world, 2, 4, 2);
    StatMod mod;
    mod.stat  = static_cast<int32_t>(conf::battle::BuffStat::ComboExpScale);
    mod.value = Fixed::fromInt(1);
    world.ecs().get<StatsComponent>(attacker).mods.push_back(mod);
    CHECK(world.ecs().get<StatsComponent>(attacker).finalStat(
              static_cast<int32_t>(conf::battle::BuffStat::ComboExpScale)) == Fixed::fromInt(2));

    conf::battle::EffectDef row;
    row.comboExp = "10"_fx;
    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &row, Fixed{}));
    CHECK(world.ecs().get<ComboMeterComponent>(attacker).exp == ("10"_fx * Fixed::fromInt(2)) * "1.2"_fx);
}

TEST_CASE("combo meter: timeout resets count and killedTargets, decay drops grade")
{
    World world           = makeWorld();
    const Entity attacker = buff_test::spawnActor(world, 1, 2, 4);
    const Entity target   = buff_test::spawnActor(world, 2, 4, 2);
    auto& meter           = world.ecs().get<ComboMeterComponent>(attacker);
    meter.count           = 7;
    meter.maxCount        = 12;
    meter.grade           = 2;
    meter.maxGrade        = 3;
    meter.exp             = "201"_fx;
    meter.killedTargets.push_back(target);
    world.ecs().singleton<WorldClock>().dtMs = conf::kComboTimeMs;

    runVitalsTick(world);
    CHECK(meter.count == 0);
    CHECK(meter.timerMs == Fixed{});
    CHECK(meter.killedTargets.empty());
    CHECK(meter.maxCount == 12);
    CHECK(meter.maxGrade == 3);
    CHECK(meter.grade == 2);
    CHECK(meter.exp == "201"_fx - conf::kComboStageSpeed[1]);

    meter.count = 0;
    meter.grade = 1;
    meter.exp   = "1"_fx;
    world.ecs().singleton<WorldClock>().dtMs = kLogicDtMs;
    runVitalsTick(world);
    CHECK(meter.grade == 1);
    CHECK(meter.exp == "0.5"_fx);
    runVitalsTick(world);
    CHECK(meter.grade == 1);
    CHECK(meter.exp == Fixed{});
    runVitalsTick(world);
    CHECK(meter.grade == 0);
    CHECK(meter.maxGrade == 3);
    CHECK(meter.exp == Fixed{});
}

TEST_CASE("hit feedback: recover hurt/hit EP, skip crazy, accumulate threat")
{
    World world           = makeWorld();
    const Entity attacker = buff_test::spawnActor(world, 1, 2, 4);
    const Entity target   = buff_test::spawnActor(world, 2, 4, 2);
    world.ecs().add(target, AiBrainComponent{});
    world.ecs().get<HealthComponent>(attacker).ep = {};
    world.ecs().get<HealthComponent>(target).ep   = {};

    conf::battle::EffectDef row;
    row.comboExp = "10"_fx;
    row.energy   = "20"_fx;
    const Fixed damage = Fixed::fromInt(1000);
    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &row, damage));

    CHECK(world.ecs().get<HealthComponent>(target).ep == "150"_fx);
    combat::rules::recoverEp(world, attacker, row.energy);
    CHECK(world.ecs().get<HealthComponent>(attacker).ep == "20"_fx);

    const auto& brain = world.ecs().get<AiBrainComponent>(target);
    REQUIRE(brain.threat.size() == 1);
    CHECK(brain.threat.front().source == attacker);
    CHECK(brain.threat.front().damage == damage);

    combat::rules::settleHitFeedback(world, makeCtx(attacker, target, &row, damage));
    CHECK(brain.threat.size() == 1);
    CHECK(brain.threat.front().damage == damage + damage);

    World crazyWorld           = makeWorld();
    const Entity crazyAttacker = buff_test::spawnActor(crazyWorld, 1, 2, 4);
    const Entity crazyTarget   = buff_test::spawnActor(crazyWorld, 2, 4, 2);
    crazyWorld.ecs().get<SkillCastComponent>(crazyAttacker).crazy = true;
    crazyWorld.ecs().get<SkillCastComponent>(crazyTarget).crazy   = true;
    crazyWorld.ecs().get<HealthComponent>(crazyAttacker).ep       = {};
    crazyWorld.ecs().get<HealthComponent>(crazyTarget).ep         = {};
    combat::rules::settleHitFeedback(crazyWorld, makeCtx(crazyAttacker, crazyTarget, &row, damage));
    combat::rules::recoverEp(crazyWorld, crazyAttacker, row.energy);
    CHECK(crazyWorld.ecs().get<HealthComponent>(crazyTarget).ep == Fixed{});
    CHECK(crazyWorld.ecs().get<HealthComponent>(crazyAttacker).ep == Fixed{});
    CHECK(crazyWorld.ecs().get<ComboMeterComponent>(crazyAttacker).count == 1);

    World scaleWorld           = makeWorld();
    const Entity scaleAttacker = buff_test::spawnActor(scaleWorld, 1, 2, 4);
    scaleWorld.ecs().get<HealthComponent>(scaleAttacker).ep = {};
    StatMod epMod;
    epMod.stat  = static_cast<int32_t>(conf::battle::BuffStat::EpRecoveryScale);
    epMod.value = Fixed::fromInt(1);
    scaleWorld.ecs().get<StatsComponent>(scaleAttacker).mods.push_back(epMod);
    combat::rules::recoverEp(scaleWorld, scaleAttacker, "20"_fx);
    CHECK(scaleWorld.ecs().get<HealthComponent>(scaleAttacker).ep == "40"_fx);

    World bossEpWorld           = makeWorld();
    const Entity bossEpAttacker = buff_test::spawnActor(bossEpWorld, 1, 2, 4);
    const Entity bossTarget     = buff_test::spawnActor(bossEpWorld, 4, 4, 2);
    bossEpWorld.ecs().get<HealthComponent>(bossEpAttacker).ep = {};
    const int32_t bossRole =
        bossEpWorld.ecs().has<IdentityComponent>(bossTarget) ? bossEpWorld.ecs().get<IdentityComponent>(bossTarget).roleType
                                                             : 0;
    combat::rules::recoverEp(bossEpWorld, bossEpAttacker, row.energy * (bossRole == 4 ? "1.5"_fx : "1"_fx));
    CHECK(bossEpWorld.ecs().get<HealthComponent>(bossEpAttacker).ep == "30"_fx);
}

TEST_CASE("combat system wires combo, threat and EP before AfterHit")
{
    World world           = makeWorld();
    const Entity attacker = buff_test::spawnActor(world, 1, 8, 2, 1000);
    const Entity target   = buff_test::spawnActor(world, 2, 2, 8, 1000);
    world.ecs().add(target, AiBrainComponent{});
    world.ecs().get<StatusFlagsComponent>(target).add(ExtraState::SuperArmor);
    world.ecs().get<HealthComponent>(attacker).ep = {};
    world.ecs().get<HealthComponent>(target).ep   = {};

    int32_t effectId = 0;
    for (const auto& row : conf::Config::instance().table<conf::battle::EffectDef>())
    {
        if (row.id <= 0 || row.hitId <= 0)
            continue;
        const auto* hit = conf::Config::instance().find<conf::battle::HitDef>(row.hitId);
        if (!hit || hit->hitType == -1)
            continue;
        if (hit->hitCounts != -1 && hit->hitCounts <= 0)
            continue;
        effectId = row.id;
        break;
    }
    REQUIRE(effectId > 0);
    const auto* effectRow = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
    REQUIRE(effectRow != nullptr);

    const Entity effect = world.ecs().create();
    IdentityComponent effectIdn;
    effectIdn.kind     = SpawnKind::Effect;
    effectIdn.configId = effectId;
    effectIdn.camp     = 8;
    world.ecs().add(effect, effectIdn);
    EffectInfoComponent info;
    info.configId = effectId;
    info.owner    = attacker;
    world.ecs().add(effect, info);
    world.ecs().add(effect, HitTrackerComponent{});
    world.ecs().add(effect, TransformComponent{});
    world.ecs().add(effect, PauseComponent{});

    DrawPolicy policy;
    policy.dodge = Fixed::fromInt(100);
    world.setDrawPolicy(&policy);

    ContactEvent contact;
    contact.a     = effect;
    contact.b     = target;
    contact.phase = ContactPhase::Began;
    world.ecs().singleton<EventQueue<ContactEvent>>().push(contact);
    runCombat(world);

    CHECK(world.ecs().get<ComboMeterComponent>(attacker).count == 1);
    CHECK(world.ecs().get<ComboMeterComponent>(attacker).maxCount == 1);
    const auto& brain = world.ecs().get<AiBrainComponent>(target);
    REQUIRE_FALSE(brain.threat.empty());
    CHECK(brain.threat.front().source == attacker);
    if (effectRow->energy > Fixed{})
        CHECK(world.ecs().get<HealthComponent>(attacker).ep > Fixed{});
}
