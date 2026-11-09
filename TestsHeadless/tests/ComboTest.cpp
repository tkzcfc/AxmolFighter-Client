#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim_legacy/World.h"
#include "mugen/sim_legacy/role/RoleCombo.h"
#include "mugen/sim_legacy/role/Role.h"
#include "mugen/sim_legacy/spawn/RoleSpawn.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/BattleConst.h"

namespace
{
// 创建带连击状态、外部条件与受击判定所需组件的角色实体。
mugen::Entity comboActor(mugen::sim_legacy::World& world, int32_t roleType = 1)
{
    const auto entity = world.create(mugen::sim_legacy::GroupKind::Role);
    mugen::sim_legacy::RoleIdentity identity;
    identity.roleType = roleType;
    world.ecs().add(entity, identity);
    world.ecs().add(entity, mugen::sim_legacy::RoleStatus{});
    world.ecs().add(entity, mugen::sim_legacy::ExtraState{});
    world.ecs().add(entity, mugen::sim_legacy::RoleHitState{});
    world.ecs().add(entity, mugen::sim_legacy::BuffExternalConditions{});
    mugen::sim_legacy::Vitals vitals;
    vitals.hp = vitals.maxHp = mugen::Fixed::fromInt(1000);
    world.ecs().add(entity, vitals);
    return entity;
}
}  // namespace

TEST_CASE("Combo evaluation maps experience to stage thresholds")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner  = comboActor(world, 1);
    const auto target = comboActor(world, 1);
    auto& state       = world.ecs().get<RoleHitState>(owner);

    struct Case
    {
        int32_t exp;
        int32_t level;
    };
    const Case cases[] = {{0, 1}, {200, 2}, {500, 3}, {900, 4}, {1500, 5}};
    for (const auto& c : cases)
    {
        role::RoleCombo(world, owner).reset();
        role::RoleCombo(world, owner).add(Fixed::fromInt(c.exp), target);
        INFO("exp=", c.exp);
        CHECK(state.m_curCombo == 1);
        CHECK(state.m_curEvaluate == c.level);
        CHECK(state.m_maxEvaluate == c.level);
    }
}

TEST_CASE("Combo evaluation experience caps at the maximum stage")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner  = comboActor(world, 1);
    const auto target = comboActor(world, 1);
    auto& state       = world.ecs().get<RoleHitState>(owner);

    role::RoleCombo(world, owner).reset();
    role::RoleCombo(world, owner).add(Fixed::fromInt(1500), target);
    CHECK(state.m_curEvaluate == 5);
    role::RoleCombo(world, owner).add(Fixed::fromInt(1500), target);
    CHECK(state.m_evaluateExp == conf::kComboStageExp.back());
    CHECK(state.m_curEvaluate == 5);
    CHECK(state.m_maxEvaluate == 5);
    CHECK(state.m_curCombo == 2);
}

TEST_CASE("Combo counter resets after timeout")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner  = comboActor(world, 1);
    const auto target = comboActor(world, 1);
    auto& state       = world.ecs().get<RoleHitState>(owner);

    role::RoleCombo(world, owner).reset();
    role::RoleCombo(world, owner).add(Fixed::fromInt(100), target);
    CHECK(state.m_curCombo == 1);
    world.ecs().singleton<WorldClock>().dtMs = Fixed::fromInt(1000);
    role::RoleCombo(world, owner).update();
    CHECK(state.m_curCombo == 1);
    role::RoleCombo(world, owner).update();
    CHECK(state.m_curCombo == 0);
}

TEST_CASE("Combo evaluation experience decays per frame")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner  = comboActor(world, 1);
    const auto target = comboActor(world, 1);
    auto& state       = world.ecs().get<RoleHitState>(owner);

    role::RoleCombo(world, owner).reset();
    role::RoleCombo(world, owner).add(Fixed::fromInt(1500), target);
    CHECK(state.m_curEvaluate == 5);
    const Fixed before = state.m_evaluateExp;
    role::RoleCombo(world, owner).update();
    CHECK(state.m_evaluateExp == before - conf::kComboStageSpeed[4]);
    CHECK(state.m_curEvaluate == 5);
    // 连续衰减穿过阈值后评价等级下降，最终经验耗尽清除评价。
    for (int i = 0; i < 10000 && state.m_curEvaluate > 0; ++i)
        role::RoleCombo(world, owner).update();
    CHECK(state.m_curEvaluate == 0);
}

TEST_CASE("Combo kill bonus applies once per target")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner = comboActor(world, 1);
    const auto dead  = comboActor(world, 1);
    world.ecs().get<Vitals>(dead).hp = {};
    auto& state = world.ecs().get<RoleHitState>(owner);

    role::RoleCombo(world, owner).reset();
    const Fixed base = (Fixed::fromInt(100) + conf::kComboFormulaM[0]) * conf::kComboFormulaN[0];
    role::RoleCombo(world, owner).add(Fixed::fromInt(100), dead);
    CHECK(state.m_curCombo == 1);
    CHECK(state.m_evaluateExp == base + conf::kComboKillExp);
    // 同一目标已结算击杀奖励，不再重复增加。
    role::RoleCombo(world, owner).add(Fixed::fromInt(100), dead);
    CHECK(state.m_curCombo == 2);
    CHECK(state.m_evaluateExp == (base + conf::kComboKillExp) + base);
}

TEST_CASE("Combo stun bonus adds experience")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner   = comboActor(world, 1);
    const auto stunned = comboActor(world, 1);
    world.ecs().get<mugen::sim_legacy::ExtraState>(stunned).add(mugen::ExtraState::Stun);
    auto& state = world.ecs().get<RoleHitState>(owner);

    role::RoleCombo(world, owner).reset();
    const Fixed base = (Fixed::fromInt(100) + conf::kComboFormulaM[0]) * conf::kComboFormulaN[0];
    role::RoleCombo(world, owner).add(Fixed::fromInt(100), stunned);
    CHECK(state.m_evaluateExp == base + conf::kComboStunExp);
}

TEST_CASE("Combo boss hit multiplies experience")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner = comboActor(world, 1);
    const auto boss  = comboActor(world, 4);
    auto& state      = world.ecs().get<RoleHitState>(owner);

    role::RoleCombo(world, owner).reset();
    const Fixed base = (Fixed::fromInt(100) + conf::kComboFormulaM[0]) * conf::kComboFormulaN[0];
    role::RoleCombo(world, owner).add(Fixed::fromInt(100), boss);
    CHECK(state.m_evaluateExp == base * conf::kComboBossExpRate);
}

TEST_CASE("Combo evaluation syncs to buff external conditions")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    World world;
    world.init({});
    const auto owner  = comboActor(world, 1);
    const auto target = comboActor(world, 1);
    auto& state       = world.ecs().get<RoleHitState>(owner);
    auto& external    = world.ecs().get<BuffExternalConditions>(owner);

    role::RoleCombo(world, owner).reset();
    CHECK(external.m_comboEvaluate == 0);
    role::RoleCombo(world, owner).add(Fixed::fromInt(500), target);
    CHECK(state.m_curEvaluate == 3);
    CHECK(external.m_comboEvaluate == state.m_curEvaluate);
}

TEST_CASE("Spawned role carries buff external conditions and default combo scale")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    REQUIRE(mugen::test::ensureContentVfs());
    REQUIRE(conf::Config::instance().load("/mugen/config/config.bin"));
    World world;
    world.init({});
    RoleSpawnParams params;
    params.roleId = 261;
    params.level  = 100;
    const auto entity = RoleSpawner(world).spawnRole(params);
    REQUIRE(entity.value != 0);
    CHECK(world.ecs().has<BuffExternalConditions>(entity));
    CHECK(world.ecs().has<RoleHitState>(entity));
    CHECK(world.ecs().get<RoleHitState>(entity).m_parameterExp == "1"_fx);
}
