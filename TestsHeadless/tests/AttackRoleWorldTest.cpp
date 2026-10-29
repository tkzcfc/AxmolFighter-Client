#include "SkillTestSupport.h"
#include "../support/ContentVfs.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include <iostream>

using namespace skill_test;
namespace
{
struct WorldAttack
{
    World world;
    Entity entity;
    int32_t skillId = -1;
    conf::ActionConfig action;
    std::string motionFile;
    int32_t durationMs = 0;
    uint64_t frame     = 0;

    WorldAttack()
    {
        loadConfig();
        auto* assets = AvatarAssetCache::getInstance();
        REQUIRE(mugen::test::ensureContentVfs());
        REQUIRE(assets->load("/mugen/config/avatar.bin"));
        for (const auto& skill : conf::Config::instance().table<conf::SkillAttackConfig>())
        {
            if (skill.cd != 0 || skill.actionIds.empty() || skill.actionIds.front().empty() || skill.skillType == 1)
                continue;
            const auto* row = conf::Config::instance().find<conf::ActionConfig>(skill.actionIds.front().front());
            if (!row || row->interruptFrame < 2 || row->actionScaleTime <= Fixed{} || row->loop != "1"_fx ||
                row->staticTarget != -1 || row->floor == 0 || row->obstruct == 1)
                continue;
            const auto name = std::holds_alternative<std::string>(row->action)
                                  ? std::get<std::string>(row->action)
                                  : std::to_string(std::get<int32_t>(row->action));
            for (const auto& [path, map] : assets->motionMaps)
            {
                if (path.find("mugen/motion/hero/") == std::string::npos)
                    continue;
                const auto* motion = map.findMotion(name);
                if (!motion || motion->durationMs() <= 0)
                    continue;
                const auto threshold =
                    "1000"_fx * kLogicDtSec / row->actionScaleTime * Fixed::fromInt(row->interruptFrame);
                if (Fixed::fromInt(motion->durationMs()) / row->actionScaleTime <= threshold + kLogicDtMs * "3"_fx)
                    continue;
                skillId    = skill.id;
                action     = *row;
                motionFile = path;
                durationMs = motion->durationMs();
                break;
            }
            if (skillId != -1)
                break;
        }
        REQUIRE(skillId != -1);
        entity = actor(world);
        slots(world, entity, {skillId});
        auto& ecs = world.ecs();
        MotionState motion;
        motion.motionFile = motionFile;
        ecs.add(entity, motion);
        ecs.add(entity, RoleHitState{});
        ecs.add(entity, RoleTimers{});
        ecs.add(entity, Displacement{});
        ecs.add(entity, ColliderState{});
        ecs.add(entity, ControlEventQueue{});
        ecs.add(entity, RoleBrain{});
        ecs.add(entity, RoleControl{});
        ecs.singleton<PlayerSlots>().entities[0] = entity;
    }
    void tick(bool press = false)
    {
        FrameInput input;
        input.frame = ++frame;
        if (press)
        {
            PlayerCommand command;
            command.controlEvent = ControlEvent::CTR_ATKA;
            input.players.push_back({0, {command}});
        }
        world.tick(input);
    }
    // 返回当前施法动作的唯一标识；不在攻击分支时为 -1。
    int64_t active() const
    {
        auto& brain = world.ecs().get<RoleBrain>(entity);
        if (brain.m_branch != role::brain::RoleBranch::Attack || brain.m_cast.m_skill < 0)
            return -1;
        return static_cast<int64_t>(brain.m_cast.m_skill) * 1000 + brain.m_cast.m_index;
    }
    // 返回当前攻击叶的逐帧状态。
    const attack::ActionAttack& state() const { return world.ecs().get<RoleBrain>(entity).m_cast.m_leaf.m_action; }
    void overrideAction()
    {
        conf::ConfigArchive archive;
#define COPY_WORLD_ATTACK_TABLE(kind, Type, Key) archive.kind = conf::Config::instance().table<conf::Type>();
        MG_CONFIG_TABLES(COPY_WORLD_ATTACK_TABLE)
#undef COPY_WORLD_ATTACK_TABLE
        for (auto& row : archive.action)
            if (row.id == action.id)
                row = action;
        ByteBuffer bytes;
        for (auto c : conf::kConfigMagic)
            bytes.writeByte(c);
        bytes.writeUint32(conf::kConfigVersion);
        bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter(bytes).write(archive);
        bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes({bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()}));
    }
    ~WorldAttack() { (mugen::test::ensureContentVfs() && conf::Config::instance().load("/mugen/config/config.bin")); }
};
}  // namespace

TEST_CASE("AttackRole World buffered FrameInput chains running leaf")
{
    WorldAttack f;
    f.tick(true);
    const auto first = f.active();
    REQUIRE(first >= 0);
    const auto oldSkill = f.world.ecs().get<SkillPool>(f.entity).m_current;
    REQUIRE_FALSE(f.world.ecs().get<SkillPool>(f.entity).m_allowInterrupt);
    f.tick(true);
    REQUIRE(f.world.ecs().get<SkillPool>(f.entity).m_skills.at(oldSkill).m_nextSkill != kNoSkill);
    for (int i = 0; i < 100 && f.active() == first; ++i)
        f.tick();
    REQUIRE(f.active() >= 0);
    CHECK(f.active() != first);
    CHECK(f.world.ecs().get<SkillPool>(f.entity).m_current != oldSkill);
    f.tick();
    REQUIRE(f.active() >= 0);
    auto& brain         = f.world.ecs().get<RoleBrain>(f.entity);
    const auto next     = brain.m_cast.m_skill;
    const auto actionId = brain.m_cast.m_actions[static_cast<size_t>(brain.m_cast.m_index)];
    CHECK(next != oldSkill);
    std::cout << "AttackRole World buffered skill=" << f.skillId << " first_action=" << f.action.id
              << " next_action=" << actionId << " tick=" << f.frame << '\n';
}

TEST_CASE("AttackRole World no press waits for real animation and delay")
{
    WorldAttack f;
    f.tick(true);
    const auto first = f.active();
    REQUIRE(first >= 0);
    const auto duration       = Fixed::fromInt(f.durationMs);
    const auto speed          = f.action.actionScaleTime;
    const auto animationTicks = static_cast<uint64_t>((duration / (kLogicDtMs * speed)).ceil().toInt());
    while (f.frame < animationTicks)
    {
        REQUIRE(f.active() == first);
        CHECK_FALSE(f.state().m_animationEnd);
        f.tick();
    }
    REQUIRE(f.active() == first);
    REQUIRE(f.state().m_animationEnd);
    CHECK(f.state().m_time == Fixed{});
    const auto delayTicks =
        std::max<int64_t>(1, (Fixed::fromInt(f.action.actionDelayTime) / kLogicDtMs).ceil().toInt());
    for (int64_t i = 1; i < delayTicks; ++i)
    {
        f.tick();
        REQUIRE(f.active() == first);
        CHECK(f.state().m_time < Fixed::fromInt(f.action.actionDelayTime));
    }
    f.tick();
    CHECK(f.active() != first);
    std::cout << "AttackRole World no-press action=" << f.action.id << " duration_ms=" << f.durationMs
              << " delay_ms=" << f.action.actionDelayTime << " animation_tick=" << animationTicks
              << " exit_tick=" << f.frame << '\n';
}

TEST_CASE("AttackRole World real config opens interrupt on predicted tick")
{
    WorldAttack f;
    const auto interval =
        "1000"_fx * kLogicDtSec / (f.action.actionScaleTime * f.world.ecs().get<RoleHitState>(f.entity).m_actionScale);
    const auto threshold = interval * Fixed::fromInt(f.action.interruptFrame);
    const auto predicted = static_cast<uint64_t>(threshold.raw / kLogicDtMs.raw + 1);
    REQUIRE(predicted > 1);
    f.tick(true);
    const auto node = f.active();
    REQUIRE(node >= 0);
    CHECK(f.state().m_interval == interval);
    while (f.frame < predicted)
    {
        REQUIRE(f.active() == node);
        CHECK_FALSE(f.world.ecs().get<SkillPool>(f.entity).m_allowInterrupt);
        CHECK_FALSE(f.state().m_animationEnd);
        f.tick();
    }
    CHECK(f.active() == node);
    CHECK(f.state().m_time > threshold);
    CHECK(f.state().m_time - kLogicDtMs <= threshold);
    CHECK(f.world.ecs().get<SkillPool>(f.entity).m_allowInterrupt);
    std::cout << "AttackRole World timing skill=" << f.skillId << " action=" << f.action.id
              << " predicted_tick=" << predicted << " observed_tick=" << f.frame << " threshold_raw=" << threshold.raw
              << " motion=" << f.motionFile << '\n';
}

TEST_CASE("AttackRole World control movement and facing run through pipeline")
{
    for (const int control : {0, 2})
    {
        WorldAttack f;
        f.action.control           = control;
        f.action.controlVelocity   = "2"_fx;
        f.action.actionPosType     = -1;
        f.action.actionOrientation = 1;
        f.action.floor             = -1;
        f.action.staticTarget      = -1;
        f.action.displacementId    = -1;
        if (control == 2)
        {
            const auto& rows = conf::Config::instance().table<conf::DisplacementConfig>();
            const auto it    = std::find_if(rows.begin(), rows.end(), [](const auto& d) {
                return d.id != -1 &&
                       std::all_of(d.velocity.begin(), d.velocity.end(), [](Fixed v) { return v == Fixed{}; }) &&
                       std::all_of(d.acceleration.begin(), d.acceleration.end(), [](Fixed v) { return v == Fixed{}; });
            });
            REQUIRE(it != rows.end());
            f.action.displacementId = it->id;
        }
        f.overrideAction();
        f.tick(true);
        const auto start = f.world.ecs().get<Transform>(f.entity).position;
        PlayerCommand command;
        command.controlEvent = ControlEvent::CTR_WALK;
        command.quadrant     = ControlQuadrant::DownLeft;
        f.world.tick({++f.frame, {{0, {command}}}, {}, {}});
        const auto& t = f.world.ecs().get<Transform>(f.entity);
        CHECK(t.towardX == -1);
        if (control == 0)
            CHECK(t.position.x < start.x);
        else
        {
            CHECK(t.position.x == start.x);
            CHECK(t.position.y == start.y);
            CHECK(t.position.z == start.z);
        }
    }
}

TEST_CASE("AttackRole World integrator brake completes infinite loop")
{
    WorldAttack f;
    const auto& rows = conf::Config::instance().table<conf::DisplacementConfig>();
    const auto it    = std::find_if(rows.begin(), rows.end(), [](const auto& d) {
        return d.id != -1 && std::all_of(d.velocity.begin(), d.velocity.end(), [](Fixed v) { return v == Fixed{}; }) &&
               std::all_of(d.acceleration.begin(), d.acceleration.end(), [](Fixed v) { return v == Fixed{}; });
    });
    REQUIRE(it != rows.end());
    f.action.displacementId = it->id;
    f.action.loop           = "-1"_fx;
    f.action.control        = 1;
    f.action.floor          = -1;
    f.action.actionPosType  = -1;
    f.action.staticTarget   = -1;
    f.action.interruptFrame = f.action.interruptExtraFrame = -1;
    f.overrideAction();
    f.tick(true);
    const auto node = f.active();
    REQUIRE(node >= 0);
    CHECK_FALSE(f.state().m_animationEnd);
    CHECK(f.state().m_finished);
    CHECK(f.world.ecs().get<Displacement>(f.entity).listenerNode ==
          static_cast<int32_t>(role::brain::RoleBranch::Attack));
    f.tick();
    CHECK(f.active() != node);
    std::cout << "AttackRole World brake action=" << f.action.id << " displacement=" << f.action.displacementId
              << " exit_tick=" << f.frame << '\n';
}

TEST_CASE("AttackRole World static targets expire reset and pause leaf")
{
    for (const int target : {0, 1, 2})
    {
        WorldAttack f;
        f.action.staticTarget           = target;
        f.action.staticStartFrame       = 0;
        f.action.staticTime             = 100;
        f.action.displaySpineFrameCount = 3;
        f.action.staticResetTime        = 200;
        f.action.loop                   = "-1"_fx;
        f.action.displacementId         = -1;
        f.action.control                = 1;
        f.action.floor                  = -1;
        f.action.interruptFrame = f.action.interruptExtraFrame = -1;
        f.overrideAction();
        auto& ecs      = f.world.ecs();
        const auto add = [&](EntityCamp camp) {
            const auto e = f.world.create(GroupKind::Role);
            RoleIdentity identity;
            identity.camp = static_cast<int32_t>(camp);
            ecs.add(e, identity);
            ecs.add(e, RoleTimers{});
            ecs.add(e, Vitals{});
            ecs.add(e, sim::ExtraState{});
            ecs.add(e, RoleStatus{});
            ecs.add(e, Transform{});
            ecs.add(e, RoleHitState{});
            ecs.add(e, MotionState{});
            ecs.add(e, Displacement{});
            ecs.add(e, ColliderState{});
            ecs.add(e, ControlEventQueue{});
            ecs.add(e, RoleBrain{});
            ecs.add(e, RoleControl{});
            return e;
        };
        const auto enemy = add(EntityCamp::Enemy), friendEnemy = add(EntityCamp::EnemyFriend),
                   ally = add(EntityCamp::OurFriend);
        f.tick(true);
        const auto node = f.active();
        REQUIRE(node >= 0);
        const auto duration = target == 2 ? "3"_fx * kLogicDtSec * "1000"_fx : "100"_fx;
        CHECK(ecs.get<RoleTimers>(enemy).staticMs == std::max(Fixed{}, duration - kLogicDtMs));
        CHECK(ecs.get<RoleTimers>(friendEnemy).staticMs == std::max(Fixed{}, duration - kLogicDtMs));
        CHECK(ecs.get<RoleTimers>(ally).staticMs == Fixed{});
        CHECK(ecs.get<RoleTimers>(f.entity).staticMs == (target == 0 ? Fixed{} : duration));
        const auto time = f.state().m_time;
        f.tick();
        CHECK(f.state().m_time == (target == 0 ? time + kLogicDtMs : time));
        const auto resetAt = kLogicDtMs + "200"_fx;
        while (ecs.singleton<WorldClock>().elapsedMs + kLogicDtMs < resetAt)
        {
            f.tick();
            CHECK(ecs.get<RoleTimers>(ally).staticMs == Fixed{});
        }
        CHECK(ecs.get<RoleTimers>(enemy).staticMs == Fixed{});
        f.tick();
        CHECK(ecs.get<RoleTimers>(enemy).staticMs == std::max(Fixed{}, duration - kLogicDtMs));
        std::cout << "AttackRole World static target=" << target << " duration_raw=" << duration.raw
                  << " reset_tick=" << f.frame << '\n';
    }
}
