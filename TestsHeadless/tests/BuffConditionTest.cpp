#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffHolder.h"

// 同一条件入口检查资源阈值、控制状态、绑定上下文和源顺序短路。
TEST_CASE("BuffCondition predicates read entity and event state")
{
    using namespace mugen;
    using namespace mugen::sim;
    World world;
    const auto entity = skill_test::actor(world);
    BuffInstance instance;
    instance.m_context.m_target = entity;
    conf::BuffConfig config;
    config.condition = {0, 125, 127, 145};
    config.conditionParam = {{}, {"1"_fx}, {"1"_fx}, {"1"_fx}};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().get<Vitals>(entity).hp = "500"_fx;
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.conditionParam[1] = {"-0.5"_fx};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.condition = {103};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().get<sim::ExtraState>(entity).add(mugen::ExtraState::SuperArmor);
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.condition = {238, 155};
    config.conditionParam = {{"123"_fx}, {"5"_fx}};
    instance.m_context.m_currentSkill = 123;
    instance.m_context.m_needUseTp = "5"_fx;
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    instance.m_context.m_currentSkill = 124;
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.condition = {259};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.condition = {257};
    config.conditionParam = {{}};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().add(entity, BuffPool{});
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.condition = {8};
    config.conditionParam = {{"2"_fx}};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().add(entity, BuffExternalConditions{2, true, "1"_fx});
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.conditionParam = {{"3"_fx}};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.condition = {263};
    config.conditionParam = {{"1"_fx}};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().get<BuffExternalConditions>(entity).m_angerFreezeSeconds = {};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.conditionParam = {{"0"_fx}};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().get<BuffExternalConditions>(entity).m_angerPartner = false;
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
}

// 配置槽位编号不等于运行时槽数组下标，验证搓招与普通槽的独立映射。
TEST_CASE("BuffCondition slot identities and sampled resource changes")
{
    using namespace mugen;
    using namespace mugen::sim;
    World world;
    const auto entity = skill_test::actor(world);
    SkillPool skills;
    skills.m_slots = {{"L", true}, {"6+", true}, {"B", true}};
    SkillBase selected;
    selected.m_configId = 123;
    selected.m_address.m_slot = 0;
    selected.m_cd = "100"_fx;
    skills.m_skills.push_back(selected);
    world.ecs().add(entity, skills);
    BuffInstance instance;
    instance.m_context.m_currentSkill = 123;
    instance.m_context.m_target = entity;
    conf::BuffConfig config;
    config.condition = {175};
    config.conditionParam = {{"25"_fx}};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.conditionParam = {{"1"_fx}};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.condition = {249};
    config.conditionParam = {{"16"_fx}};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.conditionParam = {{"1"_fx}};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    world.ecs().get<SkillPool>(entity).m_skills[0].m_address.m_slot = 1;
    config.condition = {243};
    config.conditionParam = {{"1"_fx}};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.conditionParam = {{"2"_fx}};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
    auto& vitals = world.ecs().get<Vitals>(entity);
    vitals.m_lostHp = true;
    vitals.m_hpChangePercent = "-0.25"_fx;
    config.condition = {154, 165, 239, 240};
    config.conditionParam = {{}, {}, {"-0.2"_fx}, {"-0.2"_fx}};
    CHECK(buff::BuffHolder(world, entity).checkConditions(instance, config));
    config.conditionParam[3] = {"-0.3"_fx};
    CHECK_FALSE(buff::BuffHolder(world, entity).checkConditions(instance, config));
}
