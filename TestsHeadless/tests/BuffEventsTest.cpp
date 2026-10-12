#include "doctest.h"
#include "mugen/sim/buff/BuffEvents.h"

// 绑定技能与目标过滤必须同时满足，重置不替换初始化事件表。
TEST_CASE("BuffEvents binding accepts only matching skill and event")
{
    using namespace mugen;
    using namespace mugen::sim;
    conf::BuffConfig config;
    config.began = 4;
    config.ended = 6;
    config.binding = 1;
    config.eventParam = {8, 0};
    BuffInstance instance;
    instance.m_context.m_boundSkill = 920000;
    BuffContext context;
    const Entity owner{1048576};
    context.m_target = owner;
    context.m_currentSkill = 920000;
    CHECK(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(4), context, 0));
    context.m_currentSkill = 920001;
    CHECK_FALSE(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(4), context, 0));
    context.m_currentSkill.reset();
    CHECK_FALSE(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(4), context, 0));
    context.m_currentSkill = 920000;
    CHECK_FALSE(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(5), context, 0));
    context.m_target = Entity{1048577};
    CHECK_FALSE(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(4), context, 0));
    instance.m_context.m_target = context.m_target;
    CHECK(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(4), context, 0));
    instance.m_context.m_target = {};
    config.eventParam[0] = 2;
    CHECK(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(4), context, 2));
    CHECK_FALSE(buff::acceptsEvent(config, config, instance, owner, static_cast<BFEvent>(4), context, 3));
    const auto initial = config;
    config.began = 5;
    config.binding = 0;
    context.m_currentSkill.reset();
    CHECK(buff::acceptsEvent(config, initial, instance, owner, static_cast<BFEvent>(4), context, 2));
    CHECK_FALSE(buff::acceptsEvent(config, initial, instance, owner, static_cast<BFEvent>(5), context, 2));
}
