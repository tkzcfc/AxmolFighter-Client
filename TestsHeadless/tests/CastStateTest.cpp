#include "SkillTestSupport.h"
#include "mugen/sim/role/Role.h"
#include "mugen/sim/role/brain/RoleBrain.h"
#include "mugen/sim/role/brain/states/CastState.h"
#include "mugen/sim/trace/TraceSink.h"
#include <string>

using namespace mugen;
using namespace mugen::sim;
using namespace skill_test;
namespace states = mugen::sim::role::brain::states;

namespace
{
// 组装一个可施法角色并挂载追踪输出，供 CastState 测试观察施法配对与顺序。
struct CastFixture
{
    World world;
    Entity entity;
    trace::TraceSink sink;
    int32_t skillId = -1;

    explicit CastFixture(int32_t id)
        : skillId(id)
    {
        loadConfig();
        world.init({});
        world.setTrace(&sink);
        entity = actor(world);
        slots(world, entity, {skillId});
        auto& ecs = world.ecs();
        ecs.add(entity, MotionState{});
        ecs.add(entity, RoleHitState{});
        ecs.add(entity, RoleTimers{});
        ecs.add(entity, Displacement{});
        ecs.add(entity, ColliderState{});
        ecs.add(entity, ControlEventQueue{});
        ecs.add(entity, RoleBrain{});
        ecs.add(entity, RoleControl{});
    }
    RoleBrain& brain() { return world.ecs().get<RoleBrain>(entity); }
    // 选择当前技能并进入施法状态，返回施法技能下标。
    int32_t beginCast()
    {
        REQUIRE(skill::SkillCaster(world, entity).presetSkill("A"));
        REQUIRE(Role(world, entity).changeStatus(EntityRoleStatus::Attack));
        REQUIRE(states::CastState::enter(brain(), world, entity));
        return world.ecs().get<SkillPool>(entity).m_current;
    }
    // 返回追踪文本中 cast / castend 行的数量。
    std::pair<int, int> castPairing() const
    {
        int began = 0, ended = 0;
        size_t pos = 0;
        while ((pos = sink.text().find("cast ", pos)) != std::string::npos)
        {
            ++began;
            pos += 5;
        }
        pos = 0;
        while ((pos = sink.text().find("castend ", pos)) != std::string::npos)
        {
            ++ended;
            pos += 8;
        }
        return {began, ended};
    }
};
// 找一个可施放的单动作列技能，用于完整的施法配对测试。
int32_t simpleCastSkill()
{
    return findSkill([](const auto& s) {
        return s.cd == 0 && !s.actionIds.empty() && !s.actionIds.front().empty() && s.actionIds.front().front() != -1 &&
               s.actionIds.front().size() == 1 &&
               conf::Config::instance().find<conf::ActionConfig>(s.actionIds.front().front());
    });
}
// 找一个带后续技能链且可打断的技能，用于续接测试。
int32_t chainedCastSkill()
{
    return findSkill([](const auto& s) {
        return s.cd == 0 && s.nextSkill != -1 && conf::Config::instance().find<conf::SkillAttackConfig>(s.nextSkill) &&
               !s.actionIds.empty() && !s.actionIds.front().empty() && s.actionIds.front().front() != -1 &&
               conf::Config::instance().find<conf::ActionConfig>(s.actionIds.front().front()) &&
               conf::Config::instance().get<conf::ActionConfig>(s.actionIds.front().front()).interruptFrame >= 0;
    });
}
}  // namespace

TEST_CASE("CastState builds a plan from the current skill")
{
    const auto id = simpleCastSkill();
    CastFixture f(id);
    auto& pool   = f.world.ecs().get<SkillPool>(f.entity);
    REQUIRE(skill::SkillCaster(f.world, f.entity).presetSkill("A"));
    const auto index      = pool.m_current;
    const auto pipeBefore = pool.m_skills.at(index).m_pipe;
    const auto& row =
        conf::Config::instance().get<conf::SkillAttackConfig>(pool.m_skills.at(index).m_configId);
    REQUIRE(Role(f.world, f.entity).changeStatus(EntityRoleStatus::Attack));
    REQUIRE(states::CastState::enter(f.brain(), f.world, f.entity));
    auto& plan = f.brain().m_cast;
    CHECK(plan.m_skill == index);
    CHECK(plan.m_index == 0);
    CHECK(plan.m_pipe == pipeBefore);
    CHECK(plan.m_actions == row.actionIds.front());
    CHECK(f.castPairing() == std::pair{1, 0});
}

TEST_CASE("CastState pairs cast/castend and resets skill on completion")
{
    CastFixture f(simpleCastSkill());
    f.beginCast();
    auto& pool = f.world.ecs().get<SkillPool>(f.entity);
    REQUIRE(f.castPairing() == std::pair{1, 0});
    // 结束动作（直接置完成标记），推进一帧。
    f.brain().m_cast.m_leaf.m_action.m_finished = true;
    CHECK(states::CastState::update(f.brain(), f.world, f.entity) == role::brain::StateStep::Done);
    CHECK(f.castPairing() == std::pair{1, 1});
    CHECK(f.sink.text().find("cast ") < f.sink.text().find("castend "));
    CHECK(pool.m_current == kNoSkill);
}

TEST_CASE("CastState continues into the queued step when the pipe ends")
{
    const auto id = chainedCastSkill();
    CastFixture f(id);
    auto& pool = f.world.ecs().get<SkillPool>(f.entity);
    REQUIRE(pool.m_skills.size() >= 2);
    const auto index = f.beginCast();
    // 队列下一步并逐动作推进到列尾；完成最后一动作时经 dealWithNextSkillBase 续接。
    pool.m_skills[index].m_nextSkill = index + 1;
    auto& plan                       = f.brain().m_cast;
    for (;;)
    {
        const bool last = plan.m_index + 1 >= static_cast<int32_t>(plan.m_actions.size());
        plan.m_leaf.m_action.m_finished = true;
        const auto step                 = states::CastState::update(f.brain(), f.world, f.entity);
        if (last)
        {
            CHECK(step == role::brain::StateStep::Running);
            break;
        }
        CHECK(step == role::brain::StateStep::Running);
        CHECK(plan.m_skill == index);
    }
    CHECK(f.brain().m_cast.m_skill == index + 1);
    CHECK(pool.m_current == index + 1);
}

TEST_CASE("CastState opens a new plan when the interrupt window advances the skill")
{
    const auto id = chainedCastSkill();
    CastFixture f(id);
    auto& pool = f.world.ecs().get<SkillPool>(f.entity);
    REQUIRE(pool.m_skills.size() >= 2);
    const auto index = f.beginCast();
    const auto& row  = conf::Config::instance().get<conf::ActionConfig>(f.brain().m_cast.m_actions.front());
    REQUIRE(row.interruptFrame >= 0);
    // 队列下一步并在打断窗口内推进时间，触发打断续接。
    pool.m_skills[index].m_nextSkill = index + 1;
    auto& leaf                      = f.brain().m_cast.m_leaf.m_action;
    leaf.m_time = leaf.m_interval * Fixed::fromInt(row.interruptFrame) + Fixed::fromRaw(1);
    // 首帧：打断切换技能并进入桥接动作（旧列下一动作）。
    CHECK(states::CastState::update(f.brain(), f.world, f.entity) == role::brain::StateStep::Running);
    CHECK(pool.m_current != index);
    CHECK(f.castPairing() == std::pair{1, 0});
    // 次帧：检测到技能已切换，结束旧管线并进入新技能。
    CHECK(states::CastState::update(f.brain(), f.world, f.entity) == role::brain::StateStep::Running);
    CHECK(f.brain().m_cast.m_skill == index + 1);
    CHECK(f.castPairing() == std::pair{2, 1});
}
