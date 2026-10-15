#include "SkillTestSupport.h"
#include "mugen/sim/attack/AttackRole.h"
#include "mugen/sim/attack/ActionAttack.h"

using namespace skill_test;
namespace
{
struct Fixture
{
    World world;
    Entity entity;
    attack::AttackLeafParam param;
    attack::AttackRole state;
    conf::ActionConfig row;
    Fixture()
    {
        loadConfig();
        const auto id = findSkill([](const auto& s) {
            return s.cd == 0 && s.nextSkill != -1 &&
                   conf::Config::instance().find<conf::SkillAttackConfig>(s.nextSkill) && !s.actionIds.empty() &&
                   !s.actionIds.front().empty() &&
                   conf::Config::instance().find<conf::ActionConfig>(s.actionIds.front().front()) &&
                   conf::Config::instance().get<conf::ActionConfig>(s.actionIds.front().front()).interruptFrame >= 0;
        });
        row           = conf::Config::instance().get<conf::ActionConfig>(
            conf::Config::instance().get<conf::SkillAttackConfig>(id).actionIds.front().front());
        entity = actor(world);
        slots(world, entity, {id});
        auto& ecs = world.ecs();
        ecs.add(entity, MotionState{});
        ecs.add(entity, RoleHitState{});
        ecs.add(entity, RoleTimers{});
        ecs.add(entity, Displacement{});
        ecs.add(entity, ColliderState{});
        ecs.add(entity, RoleBrain{});
        ecs.add(entity, RoleControl{});
        param                                = attack::AttackLeafParam{row.id, 1, 0};
        ecs.get<SkillPool>(entity).m_current = 0;
    }
    void replace()
    {
        conf::ConfigArchive archive;
#define COPY_ATTACK_TABLE(kind, Type, Key) archive.kind = conf::Config::instance().table<conf::Type>();
        MG_CONFIG_TABLES(COPY_ATTACK_TABLE)
#undef COPY_ATTACK_TABLE
        for (auto& r : archive.action)
            if (r.id == row.id)
                r = row;
        ByteBuffer bytes;
        for (auto c : conf::kConfigMagic)
            bytes.writeByte(c);
        bytes.writeUint32(conf::kConfigVersion);
        bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter(bytes).write(archive);
        bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes({bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()}));
    }
    void enter()
    {
        state = attack::AttackRole{};
        role::brain::RoleContext rc{world, entity};
        attack::AttackAction::enter(rc, param, state);
    }
    BTStatus tick(Fixed time)
    {
        world.ecs().singleton<WorldClock>().dtMs = {};
        state.m_action.m_time                    = time;
        role::brain::RoleContext rc{world, entity};
        return attack::AttackAction::update(rc, param, state);
    }
    ~Fixture() { conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin"); }
};
}  // namespace
TEST_CASE("AttackRole real config interrupt boundary is strict")
{
    Fixture f;
    REQUIRE(f.row.interruptFrame >= 0);
    f.enter();
    const auto interval = "1000"_fx * kLogicDtSec / f.row.actionScaleTime;
    CHECK(f.state.m_action.m_interval == interval);
    const auto threshold = interval * Fixed::fromInt(f.row.interruptFrame);
    f.tick(threshold);
    CHECK_FALSE(f.world.ecs().get<SkillPool>(f.entity).m_allowInterrupt);
    f.tick(threshold + Fixed::fromRaw(1));
    CHECK(f.world.ecs().get<SkillPool>(f.entity).m_allowInterrupt);
}
TEST_CASE("AttackRole buffered press chains and no press waits for animation delay")
{
    Fixture f;
    f.row.interruptFrame      = 2;
    f.row.interruptExtraFrame = -1;
    f.row.actionDelayTime     = 100;
    f.replace();
    f.enter();
    const auto threshold = f.state.m_action.m_interval * "2"_fx;
    CHECK(f.tick(threshold + "1"_fx) == BTStatus::Running);
    auto& pool = f.world.ecs().get<SkillPool>(f.entity);
    REQUIRE(pool.m_skills.size() >= 2);
    pool.m_skills[0].m_nextSkill = 1;
    CHECK(f.tick(threshold + "1"_fx) == BTStatus::Success);
    CHECK(pool.m_current == 1);
    pool.m_current = 0;
    f.enter();
    CHECK(f.tick("10000"_fx) == BTStatus::Running);
    auto& action          = f.state.m_action;
    action.m_animationEnd = true;
    CHECK(f.tick("99"_fx) == BTStatus::Running);
    CHECK(f.tick("100"_fx) == BTStatus::Success);
}
TEST_CASE("AttackRole control zero moves control two only turns")
{
    Fixture f;
    f.row.control         = 0;
    f.row.displacementId  = -1;
    f.row.controlVelocity = "2"_fx;
    f.replace();
    f.enter();
    auto& ecs                                    = f.world.ecs();
    ecs.get<RoleControl>(f.entity).m_last.quadrant = 3;
    f.tick({});
    CHECK(ecs.get<RoleHitState>(f.entity).forceVelocity.x == "-2"_fx);
    CHECK(ecs.get<Transform>(f.entity).towardX == -1);
    f.row.control        = 2;
    f.row.displacementId = 0;
    f.replace();
    f.enter();
    ecs.get<RoleControl>(f.entity).m_last.quadrant = 1;
    ecs.get<RoleHitState>(f.entity).forceVelocity = {};
    f.tick({});
    CHECK(ecs.get<Transform>(f.entity).towardX == 1);
    CHECK(ecs.get<RoleHitState>(f.entity).forceVelocity.x == Fixed{});
}
TEST_CASE("AttackRole static targets preserve allies and choose durations")
{
    for (int target = 0; target <= 2; ++target)
    {
        Fixture f;
        f.row.staticTarget           = target;
        f.row.staticTime             = 123;
        f.row.staticStartFrame       = 2;
        f.row.displaySpineFrameCount = 9;
        f.row.staticResetTime        = 1000;
        f.replace();
        f.enter();
        auto& ecs        = f.world.ecs();
        const auto enemy = f.world.create(GroupKind::Role), ally = f.world.create(GroupKind::Role);
        RoleIdentity identity;
        identity.camp = static_cast<int32_t>(EntityCamp::Enemy);
        ecs.add(enemy, identity);
        ecs.add(enemy, RoleTimers{});
        identity.camp = static_cast<int32_t>(EntityCamp::OurFriend);
        ecs.add(ally, identity);
        ecs.add(ally, RoleTimers{});
        const auto threshold = f.state.m_action.m_interval * "2"_fx;
        f.tick(threshold - Fixed::fromRaw(1));
        CHECK(ecs.get<RoleTimers>(enemy).staticMs == Fixed{});
        f.tick(threshold);
        const auto duration = target == 2 ? "9"_fx * kLogicDtSec * "1000"_fx : "123"_fx;
        CHECK(ecs.get<RoleTimers>(enemy).staticMs == duration);
        CHECK(ecs.get<RoleTimers>(ally).staticMs == Fixed{});
        CHECK(ecs.get<RoleTimers>(f.entity).staticMs == (target == 0 ? Fixed{} : duration));
    }
}
TEST_CASE("AttackRole infinite loop brake and displacement boundary policy")
{
    Fixture f;
    f.row.loop           = "-1"_fx;
    f.row.displacementId = 0;
    f.replace();
    f.enter();
    role::brain::RoleContext ctx{f.world, f.entity};
    ctx.setEvent(DisplacementEvent::Brake);
    attack::AttackAction::onDisplacement(ctx, f.param, f.state);
    CHECK(f.tick({}) == BTStatus::Success);
    for (const int obstruct : {-1, 0, 1})
    {
        f.row.obstruct = obstruct;
        f.replace();
        f.enter();
        ctx.setEvent(DisplacementEvent::Left);
        CHECK(attack::AttackAction::onDisplacement(ctx, f.param, f.state) == (obstruct == 0));
        CHECK(f.state.m_action.m_finished == (obstruct == 1));
    }
    f.row.floor = 0;
    f.replace();
    f.enter();
    ctx.setEvent(DisplacementEvent::Floor);
    CHECK(attack::AttackAction::onDisplacement(ctx, f.param, f.state));
    CHECK(f.tick({}) == BTStatus::Success);
}

TEST_CASE("AttackRole animation completion counts loops and resets delay once")
{
    Fixture f;
    f.row.loop            = "3"_fx;
    f.row.actionDelayTime = 100;
    attack::ActionAttack action;
    action.m_time = "200"_fx;
    action.completeAnimation(f.row, 2);
    CHECK_FALSE(action.m_animationEnd);
    CHECK(action.m_time == "200"_fx);
    action.completeAnimation(f.row, 3);
    CHECK(action.m_animationEnd);
    CHECK(action.m_time == Fixed{});
    action.m_time = "99"_fx;
    action.completeAnimation(f.row, 4);
    CHECK(action.m_time == "99"_fx);
    action.dealWithTime(f.row);
    CHECK_FALSE(action.m_finished);
    action.m_time = "100"_fx;
    action.dealWithTime(f.row);
    CHECK(action.m_finished);
}

TEST_CASE("AttackRole World input enters cast plan and freeze preserves progress")
{
    Fixture f;
    f.row.staticTarget   = -1;
    f.row.floor          = -1;
    f.row.displacementId = -1;
    f.replace();
    auto& ecs = f.world.ecs();
    ecs.add(f.entity, ControlEventQueue{});
    ecs.singleton<PlayerSlots>().entities[0] = f.entity;
    ecs.get<SkillPool>(f.entity).m_current   = kNoSkill;
    PlayerCommand command;
    command.controlEvent = ControlEvent::CTR_ATKA;
    f.world.tick({1, {{0, {command}}}, {}, {}});
    auto& brain = ecs.get<RoleBrain>(f.entity);
    REQUIRE(brain.m_branch == role::brain::RoleBranch::Attack);
    REQUIRE(brain.m_cast.m_skill >= 0);
    REQUIRE(brain.m_cast.m_index == 0);
    const auto time = brain.m_cast.m_leaf.m_action.m_time;
    CHECK(time == kLogicDtMs);
    ecs.get<RoleTimers>(f.entity).freezeMs = kLogicDtMs * "3"_fx;
    f.world.tick({2, {}, {}, {}});
    CHECK(brain.m_cast.m_leaf.m_action.m_time == time);
    CHECK(ecs.get<Displacement>(f.entity).listenerNode == static_cast<int32_t>(role::brain::RoleBranch::Attack));
    f.world.tick({3, {}, {}, {}});
    f.world.tick({4, {}, {}, {}});
    CHECK(brain.m_cast.m_leaf.m_action.m_time == time + kLogicDtMs);
}
