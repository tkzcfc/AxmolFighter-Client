#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffEvents.h"
#include "mugen/sim/buff/BuffPool.h"

namespace
{
// 能力回调记录真实输入并删除实例，验证调用者不会继续使用旧指针。
void consumeBoundAbility(mugen::sim::World& world, mugen::Entity entity, int32_t id,
                         mugen::BFEvent event, const mugen::sim::BuffContext& context)
{
    using namespace mugen::sim;
    CHECK(id > 0);
    CHECK(static_cast<int32_t>(event) > 0);
    auto& pool = world.ecs().get<BuffPool>(entity);
    REQUIRE(pool.m_instances.size() == 1);
    CHECK_FALSE(pool.m_instances.front().m_lifecycle.m_active);
    CHECK(pool.m_instances.front().m_context.m_currentSkill == context.m_currentSkill);
    world.emit(entity, Tips{"Bound ability invoked"});
    mugen::sim::buff::destroyBuff(world, entity);
}
}

// 使用生产绑定能力配置，回调删除后事件广播必须安全返回。
TEST_CASE("BuffAbility binding callback precedes activation and may destroy instance")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    const auto& rows = conf::Config::instance().table<conf::BuffConfig>();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const auto& value) {
        return value.bindSpecialAbilityId > 0 && value.began > 0 && value.began < 50 &&
               value.began != 9 && value.began != 10;
    });
    REQUIRE(row != rows.end());
    WorldHooks hooks;
    hooks.m_hit.m_buffAbility = consumeBoundAbility;
    World world(hooks);
    const auto entity = skill_test::actor(world);
    BuffInstance instance;
    instance.m_id = instance.m_initialId = row->id;
    instance.m_serial = 1;
    instance.m_context.m_boundSkill = 920000;
    instance.m_context.m_target = entity;
    BuffPool pool;
    pool.m_instances.push_back(instance);
    world.ecs().add(entity, pool);
    BuffContext context;
    context.m_target = entity;
    context.m_currentSkill = 920000;
    buff::dispatchEvent(world, entity, static_cast<BFEvent>(row->began), context, 8);
    CHECK(world.ecs().get<BuffPool>(entity).m_instances.empty());
    const auto& events = world.ecs().singleton<PresentationEvents>().events;
    CHECK(std::any_of(events.begin(), events.end(), [](const auto& event) {
        const auto* tips = std::get_if<Tips>(&event.payload);
        return tips && tips->text == "Bound ability invoked";
    }));
}
