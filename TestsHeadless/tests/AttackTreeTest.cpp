#include "SkillTestSupport.h"
#include "mugen/sim/bt/RoleTree.h"
#include "mugen/sim/role/brain/RoleBrainRunner.h"

using namespace skill_test;

namespace
{
void prepareTree(World& world, Entity entity)
{
    world.ecs().add(entity, MotionState{});
    world.ecs().add(entity, RoleHitState{});
    world.ecs().add(entity, ControlEventQueue{});
    world.ecs().add(entity, Displacement{});
    RoleBrain brain;
    brain.m_attackLayout = behavior::definitionKey(world, entity);
    world.ecs().add(entity, std::move(brain));
    world.ecs().add(entity, RoleControl{});
}

void enterAttack(World& world, Entity entity)
{
    auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(skill::presetSkill(world, entity, "A"));
    REQUIRE(status::dealWithStatus(world, entity, EntityRoleStatus::Attack));
    prepareTree(world, entity);
    role::brain::update(world.ecs().get<RoleBrain>(entity), world, entity, false);
    CHECK(pool.m_current != kNoSkill);
}
}  // namespace

TEST_CASE("AttackTree A1 A2 builds slot step pipe sequence")
{
    loadConfig();
    const auto id = findSkill([](const auto& row) {
        return row.cd == 0 && row.nextSkill != -1 &&
               conf::Config::instance().find<conf::SkillAttackConfig>(row.nextSkill);
    });
    World world;
    const auto entity = actor(world);
    slots(world, entity, {id});
    const auto key = behavior::definitionKey(world, entity);
    const auto def = behavior::attackDefinition(key);
    CHECK(key.skills.size() >= 2);
    CHECK(def.topology().node(0).childCount == 1);
    const auto slot = def.topology().child(0, 0);
    CHECK(def.topology().node(slot).childCount >= 2);
}

TEST_CASE("AttackTree pipe cast lifecycle runs once and clears attack on failure")
{
    loadConfig();
    const auto id = findSkill([](const auto& row) {
        return row.cd == 0 && row.cdCount <= 1 &&
               std::any_of(row.actionIds.begin(), row.actionIds.end(),
                           [](const auto& actions) { return !actions.empty() && actions.front() != -1; });
    });
    World world;
    const auto entity = actor(world);
    slots(world, entity, {id});
    enterAttack(world, entity);
    auto& pool = world.ecs().get<SkillPool>(entity);
    CHECK(pool.m_running.size() == 1);
    const auto current                                = pool.m_current;
    world.ecs().get<RoleStatus>(entity).eEntityStatus = EntityRoleStatus::Idle;
    role::brain::update(world.ecs().get<RoleBrain>(entity), world, entity, false);
    CHECK(pool.m_running.size() == 1);
    CHECK(pool.m_current == kNoSkill);
    CHECK_FALSE(status::isAttackStatus(world.ecs().get<RoleStatus>(entity)));
    CHECK(current != kNoSkill);
}

TEST_CASE("AttackTree release on lift selects up action branch")
{
    loadConfig();
    const auto id   = findSkill([](const auto& row) {
        return std::any_of(row.actionIds.begin(), row.actionIds.end(),
                             [](const auto& actions) { return !actions.empty() && actions.front() != -1; });
    });
    auto row        = conf::Config::instance().get<conf::SkillAttackConfig>(id);
    row.skillType   = 1;
    row.upActionIds = row.actionIds;
    row.pressTime   = 1;
    ConfigOverride config({row});
    World world;
    const auto entity = actor(world);
    slots(world, entity, {id});
    auto& pool = world.ecs().get<SkillPool>(entity);
    REQUIRE(skill::presetSkill(world, entity, "A"));
    pool.m_skills.at(pool.m_current).m_upTouch = true;
    REQUIRE(status::dealWithStatus(world, entity, EntityRoleStatus::Attack));
    prepareTree(world, entity);
    role::brain::update(world.ecs().get<RoleBrain>(entity), world, entity, false);
    CHECK(pool.m_skills.at(pool.m_current).m_enoughRelease);
}

TEST_CASE("AttackTree identical skill sources produce identical keys")
{
    loadConfig();
    const auto id = findSkill([](const auto& row) { return row.cd == 0 && !row.actionIds.empty(); });
    World world;
    const auto first  = actor(world);
    const auto second = actor(world);
    slots(world, first, {id});
    slots(world, second, {id});
    const auto firstKey  = behavior::definitionKey(world, first);
    const auto secondKey = behavior::definitionKey(world, second);
    CHECK(firstKey == secondKey);
}
