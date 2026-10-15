#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffHolder.h"
#include "mugen/sim/hit/HitResolver.h"

// 命中事件必须通过真实接缝过滤绑定技能，再激活池内实例。
TEST_CASE("BuffIntegration hit trigger binding gates activation")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, Attributes{});
    world.ecs().add(entity, Displacement{});
    world.ecs().add(entity, SkillPool{});
    world.ecs().add(entity, RoleTimers{});
    BuffContext source;
    source.m_boundSkill = 920000;
    hit::HitResolver(world, entity).addEntityBuff(1, 920000, 42);
    REQUIRE(world.ecs().has<BuffPool>(entity));
    REQUIRE(world.ecs().get<BuffPool>(entity).m_instances.size() == 1);
    const auto serial = std::optional<uint64_t>{world.ecs().get<BuffPool>(entity).m_instances.front().m_serial};
    CHECK(buff::BuffHolder(world, entity).findInstance(*serial)->m_source == 42);
    REQUIRE_FALSE(buff::BuffHolder(world, entity).findInstance(*serial)->m_lifecycle.m_active);
    HitTrigger event;
    event.m_target = Entity{1048577};
    event.m_baseSkillId = 920001;
    hit::HitResolver(world, entity).trigger(BFEvent::AfterHit, event);
    CHECK_FALSE(buff::BuffHolder(world, entity).findInstance(*serial)->m_lifecycle.m_active);
    event.m_baseSkillId = 920000;
    hit::HitResolver(world, entity).trigger(BFEvent::AfterHit, event);
    CHECK(buff::BuffHolder(world, entity).findInstance(*serial)->m_lifecycle.m_active);
    CHECK(buff::BuffHolder(world, entity).findInstance(*serial)->m_context.m_currentSkill == 920000);
}

// 禁止周期更新的实例即使初始执行已满次数，也不能在池更新时被移层。
TEST_CASE("BuffIntegration negative interval preserves exhausted finite instance")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    BuffPool pool;
    BuffInstance instance;
    instance.m_serial = 1;
    instance.m_id = instance.m_initialId = 201;
    instance.m_lifecycle.m_active = true;
    instance.m_lifecycle.m_maxInterval = "-1"_fx;
    instance.m_lifecycle.m_maxTimes = 1;
    instance.m_lifecycle.m_times = 1;
    pool.m_instances.push_back(instance);
    world.ecs().add(entity, pool);
    buff::BuffHolder(world, entity).update();
    REQUIRE(buff::BuffHolder(world, entity).hasBuff(201));
    const auto* result = buff::BuffHolder(world, entity).findInstance(1);
    REQUIRE(result != nullptr);
    CHECK(result->m_lifecycle.m_repeat == 1);
    CHECK(result->m_lifecycle.m_times == 1);
}

// 眩晕通过生产规则中断当前技能并停止移动，移除后恢复速度。
TEST_CASE("BuffIntegration stun interrupts skill and restores movement")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, Displacement{});
    SkillPool skills;
    skills.m_slots.push_back({"A", true});
    SkillBase skill;
    skill.m_configId = 920000;
    skills.m_skills.push_back(skill);
    skills.m_current = 0;
    world.ecs().add(entity, skills);
    const auto& rows = conf::Config::instance().table<conf::BuffConfig>();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const auto& value) {
        const auto* rule = conf::Config::instance().find<conf::BuffRuleConfig>(value.ruleId);
        return rule && rule->className == "BuffStun";
    });
    REQUIRE(row != rows.end());
    BuffPool pool;
    BuffInstance instance;
    instance.m_id = row->id;
    instance.m_serial = 1;
    pool.m_instances.push_back(instance);
    world.ecs().add(entity, pool);
    buff::BuffHolder(world, entity).invokeRule(1, buff::RuleHook::Enter);
    CHECK(world.ecs().get<SkillPool>(entity).m_current == kNoSkill);
    CHECK(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Stun));
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.x == Fixed{});
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.z == Fixed{});
    buff::BuffHolder(world, entity).destroyBuff();
    CHECK_FALSE(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Stun));
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.x == "1"_fx);
    CHECK(world.ecs().get<Displacement>(entity).velocityScale.z == "1"_fx);
}

// 生产添加入口使用真实无敌配置，普通移除和更新共同撤销状态。
TEST_CASE("BuffIntegration real invincibility application removal and update")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, RoleHitState{});
    hit::HitResolver(world, entity).addEntityBuff(201);
    REQUIRE(world.ecs().has<BuffPool>(entity));
    REQUIRE(world.hitHooks().m_hasBuff(world, entity, 201));
    CHECK(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Invincible));
    buff::BuffHolder(world, entity).removeBuff(201, true);
    buff::BuffHolder(world, entity).update();
    CHECK_FALSE(world.hitHooks().m_hasBuff(world, entity, 201));
    CHECK_FALSE(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Invincible));
}

// 技能和命中接缝都必须进入同一个池，而非只记录请求。
TEST_CASE("BuffIntegration hit application reaches pool and clones deterministically")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    world.ecs().add(entity, RoleHitState{});
    HitBuffApplication application;
    application.m_buffId = "201"_fx;
    hit::HitResolver(world, entity).dealBuff(application);
    REQUIRE(buff::BuffHolder(world, entity).hasBuff(201));
    auto clone = world.clone();
    for (int frame = 0; frame < 100; ++frame)
    {
        buff::BuffHolder(world, entity).update();
        buff::BuffHolder(clone, entity).update();
        REQUIRE(world.hash() == clone.hash());
    }
    buff::BuffHolder(world, entity).destroyBuff();
    CHECK_FALSE(world.ecs().get<sim::ExtraState>(entity).active(mugen::ExtraState::Invincible));
    CHECK(world.ecs().get<BuffPool>(entity).m_instances.empty());
}
