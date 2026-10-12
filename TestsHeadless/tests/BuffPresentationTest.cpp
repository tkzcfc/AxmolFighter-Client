#include "doctest.h"
#include "SkillTestSupport.h"
#include "mugen/sim/buff/BuffPresentation.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

namespace
{
// 测试临时替换一个逻辑动作，作用域结束恢复原缓存项。
struct MotionOverride
{
    std::string m_path;
    std::optional<mugen::MotionMap> m_previous;
    explicit MotionOverride(std::string path, const mugen::MotionMap& map) : m_path(std::move(path))
    {
        auto& maps = mugen::AvatarAssetCache::getInstance()->motionMaps;
        const auto found = maps.find(m_path);
        if (found != maps.end()) m_previous = found->second;
        maps[m_path] = map;
    }
    ~MotionOverride()
    {
        auto& maps = mugen::AvatarAssetCache::getInstance()->motionMaps;
        if (m_previous) maps[m_path] = *m_previous;
        else maps.erase(m_path);
    }
};
}

// 正时长通过正式路径解析并创建定时器，不直接注入退出载荷。
TEST_CASE("BuffPresentation resolved motion schedules appearance and disappearance")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    const auto& spine = conf::Config::instance().get<conf::ResSpineConfig>(2214);
    CombatTimeline timeline;
    timeline.duration = 50;
    MotionMap map;
    map.defs = {{"appear", {{"appear", MotionEntryType::kSpine, spine.fileName, "test.box"}}},
                {"disappear", {{"disappear", MotionEntryType::kSpine, spine.fileName, "test.box"}}}};
    REQUIRE(map.bindTimelines([&](const std::string&) { return &timeline; }));
    MotionOverride override(AvatarPaths::motionFileFromSpine(spine.fileName), map);
    REQUIRE(buff::disappearDuration(2214) == "50"_fx);
    World world;
    const auto entity = skill_test::actor(world);
    BuffPool pool;
    BuffInstance instance;
    instance.m_id = instance.m_initialId = 25;
    instance.m_serial = 1;
    pool.m_instances.push_back(instance);
    world.ecs().add(entity, pool);
    buff::beginPresentation(world, entity, 1);
    REQUIRE(world.ecs().singleton<LogicTimers>().entries.size() == 1);
    CHECK(world.ecs().singleton<LogicTimers>().entries.front().intervalMs == "50"_fx);
    updateLogicTimers(world);
    updateLogicTimers(world);
    CHECK(std::get<BuffSpine>(world.ecs().singleton<PresentationEvents>().events.back().payload).m_stage == 2);
    buff::exitInstance(world, entity, 1);
    REQUIRE(world.ecs().singleton<LogicTimers>().entries.size() == 1);
    CHECK_FALSE(buff::findInstance(world, entity, 1)->m_lifecycle.m_destroy);
    updateLogicTimers(world);
    CHECK_FALSE(buff::findInstance(world, entity, 1)->m_lifecycle.m_destroy);
    updateLogicTimers(world);
    CHECK(buff::findInstance(world, entity, 1)->m_lifecycle.m_destroy);
}

// 持续阶段定时器只切换表现，退出会取消尚未完成的出现阶段。
TEST_CASE("BuffPresentation continuation timer is cancelled by exit")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    BuffPool pool;
    BuffInstance instance;
    instance.m_id = instance.m_initialId = 25;
    instance.m_serial = 1;
    pool.m_instances.push_back(instance);
    world.ecs().add(entity, pool);
    world.ecs().singleton<LogicTimers>().add(entity, kLogicDtMs, false, BuffContinue{1});
    updateLogicTimers(world);
    const auto& events = world.ecs().singleton<PresentationEvents>().events;
    REQUIRE(events.size() == 1);
    REQUIRE(std::holds_alternative<BuffSpine>(events.front().payload));
    CHECK(std::get<BuffSpine>(events.front().payload).m_stage == 2);
    CHECK_FALSE(buff::findInstance(world, entity, 1)->m_lifecycle.m_active);
    world.ecs().singleton<LogicTimers>().add(entity, kLogicDtMs, false, BuffContinue{1});
    buff::exitInstance(world, entity, 1);
    const auto& timers = world.ecs().singleton<LogicTimers>().entries;
    CHECK(std::none_of(timers.begin(), timers.end(), [](const auto& timer) {
        return std::holds_alternative<BuffContinue>(timer.payload);
    }));
    const auto eventCount = events.size();
    buff::continuePresentation(world, entity, 1);
    CHECK(events.size() == eventCount);
}

// 对生产资源执行同一映射路径，缺失动作按裁决立即退出并去重告警。
TEST_CASE("BuffPresentation production disappearance resolves or exits immediately")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    auto* assets = AvatarAssetCache::getInstance();
    assets->clearSearchPaths();
    assets->addSearchPath("res_zhcn");
    REQUIRE(assets->load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/avatar.bin"));
    World world;
    const auto entity = skill_test::actor(world);
    BuffPool pool;
    BuffInstance instance;
    instance.m_serial = 1;
    instance.m_id = 25;
    pool.m_instances.push_back(instance);
    world.ecs().add(entity, pool);
    const auto& config = conf::Config::instance().get<conf::BuffConfig>(25);
    const auto& spine = conf::Config::instance().get<conf::ResSpineConfig>(config.spineId);
    const auto path = AvatarPaths::motionFileFromSpine(spine.fileName);
    const auto duration = buff::disappearDuration(config.spineId);
    std::cout << "Buff disappearance mapping: spine=" << config.spineId << " path=" << path
              << " clip=disappear durationMs=" << (duration ? duration->toInt() : -1) << '\n';
    buff::exitInstance(world, entity, 1);
    REQUIRE(buff::findInstance(world, entity, 1)->m_lifecycle.m_exit);
    if (!duration || *duration <= Fixed{})
    {
        CHECK(buff::findInstance(world, entity, 1)->m_lifecycle.m_destroy);
        CHECK(world.ecs().singleton<LogicTimers>().entries.empty());
        const auto& events = world.ecs().singleton<PresentationEvents>().events;
        REQUIRE(events.size() >= 2);
        REQUIRE(std::holds_alternative<BuffSpine>(events.back().payload));
        CHECK(std::get<BuffSpine>(events.back().payload).m_stage == 0);
        CHECK(std::get<BuffSpine>(events.back().payload).m_serial == 1);
        CHECK(std::get<BuffSpine>(events.back().payload).m_buffId == 25);
        if (!duration)
            CHECK(world.ecs().singleton<sim::RoomState>().m_missingBuffMotionIds == std::vector<int32_t>{25});
    }
    else
    {
        CHECK_FALSE(buff::findInstance(world, entity, 1)->m_lifecycle.m_destroy);
        REQUIRE(world.ecs().singleton<LogicTimers>().entries.size() == 1);
        CHECK(world.ecs().singleton<LogicTimers>().entries[0].intervalMs == *duration);
    }
}

// 验证定时器只在到期时完成退出，克隆使用相同确定性到期时间。
TEST_CASE("BuffPresentation exit timer dispatch survives world clone")
{
    using namespace mugen;
    using namespace mugen::sim;
    skill_test::loadConfig();
    World world;
    const auto entity = skill_test::actor(world);
    BuffPool pool;
    BuffInstance instance;
    instance.m_serial = 7;
    instance.m_id = 1;
    instance.m_lifecycle.m_exit = true;
    pool.m_instances.push_back(instance);
    world.ecs().add(entity, pool);
    world.ecs().singleton<LogicTimers>().add(entity, kLogicDtMs * "2"_fx, false, BuffExit{7});
    auto clone = world.clone();
    updateLogicTimers(world);
    CHECK_FALSE(buff::findInstance(world, entity, 7)->m_lifecycle.m_destroy);
    updateLogicTimers(world);
    CHECK(buff::findInstance(world, entity, 7)->m_lifecycle.m_destroy);
    updateLogicTimers(clone);
    updateLogicTimers(clone);
    CHECK(clone.hash() == world.hash());
    CHECK(world.ecs().singleton<LogicTimers>().entries.empty());
}
