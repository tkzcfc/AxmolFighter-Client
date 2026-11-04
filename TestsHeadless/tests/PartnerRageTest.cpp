#include "SkillTestSupport.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/conf/BattleConst.h"
#include "mugen/sim/buff/BuffHolder.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/hit/HitResolver.h"
#include "mugen/sim/role/RoleActor.h"
#include "mugen/sim/spawn/RoleSpawn.h"

#include <algorithm>

using namespace skill_test;

namespace
{
Entity spawnRagePartner(World& world)
{
    world.init({});
    RoleSpawnParams params;
    params.roleId     = 107;
    params.playerSlot = 0;
    const Entity entity = RoleSpawner(world).spawnRole(params);
    REQUIRE(entity.value != 0);
    auto& vitals   = world.ecs().get<Vitals>(entity);
    vitals.maxMp   = "1000000"_fx;
    vitals.maxEp   = "1000000"_fx;
    vitals.mp      = vitals.maxMp;
    vitals.ep      = vitals.maxEp;
    auto& skills   = world.ecs().get<SkillPool>(entity);
    skills.m_crystals = 1000000;
    return entity;
}

bool hasRageEvent(const World& world, bool active)
{
    const auto& events = world.ecs().singleton<PresentationEvents>().events;
    return std::any_of(events.begin(), events.end(), [active](const PresentationEvent& event) {
        if (const auto* payload = std::get_if<PartnerRageChanged>(&event.payload))
            return payload->m_active == active;
        return false;
    });
}
}  // namespace

TEST_CASE("PartnerRage gain is granted once per cast and clamps to max")
{
    loadConfig();
    const int32_t skillId = findSkill([](const conf::SkillAttackConfig& row) {
        return row.cd == 0 && row.mp <= 0 && row.ep <= 0 && row.crystal <= 0;
    });
    auto row  = conf::Config::instance().get<conf::SkillAttackConfig>(skillId);
    row.rage  = 80;
    ConfigOverride override({row});
    World world;
    const Entity entity = actor(world);
    world.ecs().add(entity, PartnerRageState{});
    slots(world, entity, {skillId});
    auto& rage = world.ecs().get<PartnerRageState>(entity);
    rage.m_maxFreezeMs = Fixed::fromInt(15000);
    const int32_t index = skill::getGivenSkill(world.ecs().get<SkillPool>(entity), "A");
    REQUIRE(index != kNoSkill);

    skill::SkillCaster(world, entity).dealWithCastPipeBegan(index);
    hit::grantPartnerRageOnAcceptedHit(world, entity);
    CHECK(rage.m_anger == "80"_fx);
    hit::grantPartnerRageOnAcceptedHit(world, entity);
    CHECK(rage.m_anger == "80"_fx);
    skill::SkillCaster(world, entity).dealWithCastPipeEnded(index);
    hit::grantPartnerRageOnAcceptedHit(world, entity);
    CHECK(rage.m_anger == "80"_fx);

    rage.m_anger = "90"_fx;
    skill::SkillCaster(world, entity).dealWithCastPipeBegan(index);
    hit::grantPartnerRageOnAcceptedHit(world, entity);
    CHECK(rage.m_anger == conf::kPartnerRageMaxAnger);
}

TEST_CASE("PartnerRage anger cast is rejected when anger is not full")
{
    loadConfig();
    World world;
    const Entity entity = spawnRagePartner(world);
    auto* rage          = world.ecs().tryGet<PartnerRageState>(entity);
    REQUIRE(rage != nullptr);
    const auto& pool = world.ecs().get<SkillPool>(entity);
    const int32_t angerSkill = skill::getGivenSkill(pool, "P");
    REQUIRE(angerSkill != kNoSkill);
    CHECK(skill::SkillAssembly::eventSlot(ControlEvent::CTR_ATKQ) == "P");

    rage->m_anger = conf::kPartnerRageMaxAnger - "1"_fx;
    CHECK_FALSE(skill::SkillCaster(world, entity).isAllowCast(angerSkill));

    rage->m_anger = conf::kPartnerRageMaxAnger;
    CHECK(skill::SkillCaster(world, entity).isAllowCast(angerSkill));

    rage->m_transformed = true;
    CHECK_FALSE(skill::SkillCaster(world, entity).isAllowCast(angerSkill));
    rage->m_transformed    = false;
    rage->m_recoverPending = true;
    CHECK_FALSE(skill::SkillCaster(world, entity).isAllowCast(angerSkill));
}

TEST_CASE("PartnerRage transform starts at anger cast end")
{
    loadConfig();
    World world;
    const Entity entity = spawnRagePartner(world);
    auto* rage          = world.ecs().tryGet<PartnerRageState>(entity);
    REQUIRE(rage != nullptr);
    REQUIRE(rage->m_maxFreezeMs == Fixed::fromInt(15000));
    rage->m_anger = conf::kPartnerRageMaxAnger;
    auto& pool    = world.ecs().get<SkillPool>(entity);
    const int32_t angerSkill = skill::getGivenSkill(pool, "P");
    REQUIRE(angerSkill != kNoSkill);

    world.ecs().singleton<PresentationEvents>().events.clear();
    skill::SkillCaster(world, entity).dealWithCastPipeBegan(angerSkill);
    skill::SkillCaster(world, entity).dealWithCastPipeEnded(angerSkill);

    CHECK(rage->m_anger == Fixed{});
    CHECK(rage->m_freezeMs == Fixed::fromInt(15000));
    CHECK(rage->m_transformed);
    CHECK(pool.m_change == 3);
    CHECK(world.ecs().get<MotionState>(entity).m_partnerRageTransformed);
    CHECK(hasRageEvent(world, true));
    CHECK(pool.m_skills[static_cast<size_t>(angerSkill)].m_cd == Fixed{});
}

TEST_CASE("PartnerRage switches motion file on transform and revert")
{
    loadConfig();
    World world;
    const Entity entity = spawnRagePartner(world);
    auto* rage          = world.ecs().tryGet<PartnerRageState>(entity);
    REQUIRE(rage != nullptr);

    const auto& identity = world.ecs().get<RoleIdentity>(entity);
    const auto& roleRow  = conf::Config::instance().get<conf::RoleConfig>(identity.roleId);
    REQUIRE(roleRow.resSpineIdExt > 0);
    const std::string normalMotion = AvatarPaths::motionFileFromSpine(
        conf::Config::instance().get<conf::ResSpineConfig>(roleRow.resSpineId).fileName);
    const std::string transformedMotion = AvatarPaths::motionFileFromSpine(
        conf::Config::instance().get<conf::ResSpineConfig>(roleRow.resSpineIdExt).fileName);

    auto& motion = world.ecs().get<MotionState>(entity);
    CHECK(motion.motionFile == normalMotion);

    rage->m_anger = conf::kPartnerRageMaxAnger;
    const int32_t angerSkill = skill::getGivenSkill(world.ecs().get<SkillPool>(entity), "P");
    REQUIRE(angerSkill != kNoSkill);
    skill::SkillCaster(world, entity).dealWithCastPipeBegan(angerSkill);
    skill::SkillCaster(world, entity).dealWithCastPipeEnded(angerSkill);
    CHECK(motion.m_partnerRageTransformed);
    CHECK(motion.motionFile == transformedMotion);

    rage->m_freezeMs       = {};
    rage->m_recoverPending = true;
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Idle;
    role::RoleActor(world, entity).update();
    CHECK_FALSE(motion.m_partnerRageTransformed);
    CHECK(motion.motionFile == normalMotion);
}

TEST_CASE("PartnerRage condition 263 is true only while transformed")
{
    loadConfig();
    World world;
    const Entity entity = actor(world);
    world.ecs().add(entity, PartnerRageState{});
    BuffInstance instance;
    instance.m_context.m_target = entity;
    conf::BuffConfig config;
    config.condition = {263};
    config.conditionParam = {{"1"_fx}};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().get<PartnerRageState>(entity).m_transformed = true;
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().get<PartnerRageState>(entity).m_transformed = false;
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
}

TEST_CASE("PartnerRage reverts after freeze when role is idle")
{
    loadConfig();
    World world;
    const Entity entity = spawnRagePartner(world);
    auto* rage          = world.ecs().tryGet<PartnerRageState>(entity);
    REQUIRE(rage != nullptr);
    auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(!pool.m_skills.empty());
    rage->m_anger             = "40"_fx;
    rage->m_transformed       = true;
    rage->m_recoverPending    = false;
    rage->m_freezeMs          = Fixed::fromInt(15000);
    rage->m_pendingGain       = "10"_fx;
    rage->m_pendingGainActive = true;
    world.ecs().get<MotionState>(entity).m_partnerRageTransformed = true;
    world.ecs().get<RoleStatus>(entity).eEntityStatus             = EntityRoleStatus::Idle;
    pool.m_group                                               = 3;
    pool.m_change                                              = 3;
    pool.m_skills[0].m_cd                                      = Fixed::fromInt(999999);
    pool.m_skills[0].m_groupTime                               = Fixed::fromInt(999999);
    pool.m_skills[0].m_changeTime                              = Fixed::fromInt(999999);
    pool.m_skills[0].m_timing                                  = true;
    pool.m_skills[0].m_releaseCount                            = 0;
    pool.m_running.push_back(0);
    world.ecs().singleton<WorldClock>().dtMs = Fixed::fromInt(1000);
    world.ecs().singleton<WorldClock>().dtSec = Fixed::fromInt(1);
    world.ecs().singleton<PresentationEvents>().events.clear();

    for (int i = 0; i < 14; ++i)
        role::RoleActor(world, entity).update();
    CHECK(rage->m_transformed);
    CHECK(rage->m_freezeMs == Fixed::fromInt(1000));

    role::RoleActor(world, entity).update();
    CHECK_FALSE(rage->m_transformed);
    CHECK_FALSE(rage->m_recoverPending);
    CHECK(rage->m_anger == Fixed{});
    CHECK(rage->m_freezeMs == Fixed{});
    CHECK_FALSE(world.ecs().get<MotionState>(entity).m_partnerRageTransformed);
    CHECK(pool.m_group == 1);
    CHECK(pool.m_change == 1);
    CHECK(pool.m_skills[0].m_cd == Fixed{});
    CHECK(pool.m_skills[0].m_releaseCount == pool.m_skills[0].m_releaseMax);
    CHECK(pool.m_running.empty());
    CHECK(hasRageEvent(world, false));
}

TEST_CASE("PartnerRage BuffAngerAdd updates anger and freeze branches")
{
    loadConfig();
    const auto& rules = conf::Config::instance().table<conf::BuffRuleConfig>();
    const auto ruleIt = std::find_if(rules.begin(), rules.end(), [](const conf::BuffRuleConfig& row) {
        return row.className == "BuffAngerAdd";
    });
    REQUIRE(ruleIt != rules.end());
    const auto& buffs = conf::Config::instance().table<conf::BuffConfig>();
    const auto buffIt = std::find_if(buffs.begin(), buffs.end(), [ruleIt](const conf::BuffConfig& row) {
        return row.ruleId == ruleIt->id && !row.paramValue.empty();
    });
    REQUIRE(buffIt != buffs.end());

    World world;
    const Entity entity = actor(world);
    world.ecs().add(entity, BuffPool{});
    world.ecs().add(entity, PartnerRageState{});
    auto& rage = world.ecs().get<PartnerRageState>(entity);
    rage.m_maxFreezeMs = Fixed::fromInt(15000);

    BuffInstance instance;
    instance.m_id     = buffIt->id;
    instance.m_serial = 1;
    world.ecs().get<BuffPool>(entity).m_instances.push_back(instance);
    const Fixed ratio = buffIt->paramValue.front();

    rage.m_anger       = "10"_fx;
    rage.m_transformed = false;
    const Fixed expectedAnger =
        std::clamp("10"_fx + ratio * conf::kPartnerRageMaxAnger, Fixed{}, conf::kPartnerRageMaxAnger);
    buff::ruleDispatch(conf::BuffRuleId::BuffAngerAdd).m_hooks[0](world, entity, 1, {});
    CHECK(rage.m_anger == expectedAnger);

    rage.m_transformed = true;
    rage.m_freezeMs    = "3000"_fx;
    const Fixed expectedFreeze = std::clamp("3000"_fx + ratio * rage.m_maxFreezeMs, Fixed{}, rage.m_maxFreezeMs);
    buff::ruleDispatch(conf::BuffRuleId::BuffAngerAdd).m_hooks[0](world, entity, 1, {});
    CHECK(rage.m_freezeMs == expectedFreeze);
}
