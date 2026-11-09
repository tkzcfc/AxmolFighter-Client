#include "SkillTestSupport.h"
#include "mugen/sim_legacy/role/brain/RoleContext.h"

#include <array>
#include <vector>

using namespace skill_test;

namespace
{
// 以指定世界配置初始化并创建角色实体，组件与 skill_test::actor 保持一致。
mugen::Entity actorWithSetup(mugen::sim_legacy::World& world, mugen::sim_legacy::WorldSetup setup)
{
    world.init(setup);
    const auto entity = world.create(mugen::sim_legacy::GroupKind::Role);
    world.ecs().add(entity, mugen::sim_legacy::RoleIdentity{});
    world.ecs().add(entity, mugen::sim_legacy::RoleStatus{});
    world.ecs().add(entity, mugen::sim_legacy::ExtraState{});
    world.ecs().add(entity, mugen::sim_legacy::Transform{});
    mugen::sim_legacy::Vitals vitals;
    vitals.hp = vitals.maxHp = "1000"_fx;
    vitals.mp = vitals.maxMp = "1000000"_fx;
    vitals.ep = vitals.maxEp = "1000000"_fx;
    world.ecs().add(entity, vitals);
    return entity;
}

// 取 count 个互不相同的零消耗技能，便于直接施放。
std::array<int32_t, 6> takeCheapSkills()
{
    std::array<int32_t, 6> result{};
    std::vector<int32_t> used;
    for (auto& id : result)
    {
        id = findSkill([&](const conf::SkillAttackConfig& row) {
            return row.cd == 0 && row.mp <= 0 && row.ep <= 0 && row.crystal <= 0 &&
                   std::find(used.begin(), used.end(), row.id) == used.end();
        });
        used.push_back(id);
    }
    return result;
}

// 在槽位 A/B/C/I/J/K 各装配一个技能，供控制事件命中对应槽位。
void loadLetterSlots(mugen::sim_legacy::World& world, mugen::Entity entity, const std::array<int32_t, 6>& skills)
{
    skill::SkillLoadout loadout(5);
    loadout[0].resize(11);
    loadout[0][0]  = {{{skills[0], -1}}};   // A
    loadout[0][1]  = {{{skills[1], -1}}};   // B
    loadout[0][2]  = {{{skills[2], -1}}};   // C
    loadout[0][8]  = {{{skills[3], -1}}};   // I
    loadout[0][9]  = {{{skills[4], -1}}};   // J
    loadout[0][10] = {{{skills[5], -1}}};   // K
    skill::SkillCaster(world, entity).loadSkill(loadout);
    world.ecs().get<SkillPool>(entity).m_crystals = 1000000;
    world.ecs().add(entity, ControlEventQueue{});
    world.ecs().add(entity, RoleControl{});
}

// 通过控制命令消费路径派发一次按键，返回是否实际选定了技能。
bool cast(mugen::sim_legacy::World& world, mugen::Entity entity, ControlEvent event)
{
    idle(world, entity);
    world.ecs().get<ControlEventQueue>(entity).commands.push_back({event, ControlQuadrant::None, 0, 1});
    role::brain::RoleContext(world, entity).consumeControl();
    return world.ecs().get<SkillPool>(entity).m_current != kNoSkill;
}
}  // namespace

// PVP 中技能基础冷却取 pvpCd，普通场景仍取 cd。
TEST_CASE("PvpSkill cooldown uses pvpCd in PVP and cd otherwise")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    loadConfig();
    const auto id = findSkill([](const auto&) { return true; });
    auto row      = conf::Config::instance().get<conf::SkillAttackConfig>(id);
    row.cd        = 100;
    row.pvpCd     = 200;
    ConfigOverride config({row});

    {  // 普通场景使用 cd。
        World world;
        const auto entity = actorWithSetup(world, WorldSetup{});
        slots(world, entity, {id});
        const auto& pool = world.ecs().get<SkillPool>(entity);
        CHECK_FALSE(pool.m_skills.front().m_pvp);
        CHECK(pool.m_skills.front().m_coldMaxTime == Fixed::fromInt(100));
    }
    {  // PVP 场景使用 pvpCd。
        World world;
        WorldSetup setup;
        setup.m_pvp    = true;
        const auto entity = actorWithSetup(world, setup);
        slots(world, entity, {id});
        const auto& pool = world.ecs().get<SkillPool>(entity);
        CHECK(pool.m_skills.front().m_pvp);
        CHECK(pool.m_skills.front().m_coldMaxTime == Fixed::fromInt(200));
    }
}

// PVP 禁用奥义 I/J/K，但普通攻击 A/B/C 仍可施放。
TEST_CASE("PvpSkill ultimate I J K locked in PVP while A B C still cast")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    loadConfig();
    const auto skills = takeCheapSkills();

    World world;
    WorldSetup setup;
    setup.m_pvp       = true;
    const auto entity = actorWithSetup(world, setup);
    loadLetterSlots(world, entity, skills);

    CHECK(cast(world, entity, ControlEvent::CTR_ATKA));
    CHECK(cast(world, entity, ControlEvent::CTR_ATKB));
    CHECK(cast(world, entity, ControlEvent::CTR_ATKC));
    CHECK_FALSE(cast(world, entity, ControlEvent::CTR_ATKI));
    CHECK_FALSE(cast(world, entity, ControlEvent::CTR_ATKJ));
    CHECK_FALSE(cast(world, entity, ControlEvent::CTR_ATKK));
}

// 非 PVP 场景奥义 I/J/K 仍可正常施放，确认锁定仅作用于 PVP。
TEST_CASE("PvpSkill ultimate I J K cast normally outside PVP")
{
    using namespace mugen;
    using namespace mugen::sim_legacy;
    loadConfig();
    const auto skills = takeCheapSkills();

    World world;
    const auto entity = actorWithSetup(world, WorldSetup{});
    loadLetterSlots(world, entity, skills);

    CHECK(cast(world, entity, ControlEvent::CTR_ATKI));
    CHECK(cast(world, entity, ControlEvent::CTR_ATKJ));
    CHECK(cast(world, entity, ControlEvent::CTR_ATKK));
}
