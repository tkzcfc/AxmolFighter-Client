#include "SkillTestSupport.h"
#include "mugen/sim/effect/Effect.h"
#include "mugen/sim/skill/SkillTriggers.h"
#include "mugen/sim/hit/Hit.h"

using namespace skill_test;
namespace
{
struct Fixture
{
    conf::ConfigArchive archive;
    World world;
    Entity owner;
    Fixture()
    {
        loadConfig();
#define COPY_EFFECT_TABLE(kind, Type, Key) archive.kind = conf::Config::instance().table<conf::Type>();
        MG_CONFIG_TABLES(COPY_EFFECT_TABLE)
#undef COPY_EFFECT_TABLE
        WorldHooks hooks;
        hooks.update[0] = WorldHooks::noEntityUpdate;
        world = World(hooks);
        world.init({});
        owner = world.create(GroupKind::Role);
        world.ecs().add(owner, Transform{{"100"_fx, "20"_fx, "50"_fx}, -1, "-1"_fx, "1"_fx});
        world.ecs().add(owner, RoleHitState{});
        world.ecs().add(owner, RoleStatus{});
        world.ecs().add(owner, Attributes{});
        conf::ActionEffectConfig action;
        action.id = 200000001;
        action.actionScaleTime = "1"_fx;
        action.action = 0;
        action.loop = -1;
        action.floor = -1;
        action.obstruct = 0;
        action.displacementId = -1;
        action.control = -1;
        action.ghost = -1;
        action.summonId = -1;
        archive.action_effect.push_back(action);
        for (int i = 0; i < 6; ++i)
        {
            conf::EffectConfig row;
            row.id = 200000001 + i;
            row.actionIds = {action.id};
            row.positionType = -1;
            row.relativePosition = {"10"_fx, "5"_fx, "3"_fx};
            row.effectOriebtationX = -1;
            row.effectOriebtationZ = -1;
            row.nextEffectId = -1;
            row.hitId = -1;
            row.resSpineId = -1;
            row.autoRelease = i;
            archive.effect.push_back(row);
        }
    }
    void load()
    {
        ByteBuffer bytes;
        for (auto c : conf::kConfigMagic) bytes.writeByte(c);
        bytes.writeUint32(conf::kConfigVersion);
        bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter(bytes).write(archive);
        bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes({bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()}));
    }
    conf::EffectConfig& row(int index) { return archive.effect[archive.effect.size() - 6 + index]; }
    Entity spawn(int index = 0) { return effect::spawnFromRole(world, owner, {200000001 + index, 12, 11, 2}); }
    void tick() { FrameInput input; input.frame = world.ecs().singleton<WorldClock>().frame + 1; world.tick(input); }
    ~Fixture() { conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin"); }
};
}

TEST_CASE("Summon effect frame creates once and recursively uses owner standard hurt")
{
    Fixture f;
    auto summon = std::find_if(f.archive.role.begin(), f.archive.role.end(), [](const auto& row) { return row.roleType == 5; });
    REQUIRE(summon != f.archive.role.end());
    const auto summonId = summon->id;
    auto& action = f.archive.action_effect.back();
    action.summonId = summonId;
    action.summonFrame = 3;
    action.summonTime = 100;
    f.load();
    RoleIdentity identity;
    identity.level = 1;
    f.world.ecs().add(f.owner, identity);
    f.world.ecs().add(f.owner, DropGoods{{6, 7}, "25"_fx, "40"_fx});
    f.world.ecs().get<Attributes>(f.owner).basic[5] = "1234"_fx;
    const auto effect = f.spawn();
    Entity child;
    int count = 0;
    const auto collect = [&] {
        count = 0;
        f.world.ecs().each<RoleIdentity>([&](Entity entity, const RoleIdentity& role) {
            if (role.m_owner == f.owner) { child = entity; ++count; }
        });
    };
    f.tick(); f.tick(); collect();
    CHECK(count == 0);
    f.tick(); collect();
    CHECK(count == 0);
    f.tick(); collect();
    REQUIRE(count == 1);
    REQUIRE(f.world.ecs().has<DropGoods>(child));
    CHECK(f.world.ecs().get<DropGoods>(child).m_goods == std::vector<int32_t>{6, 7});
    CHECK(f.world.ecs().get<DropGoods>(child).m_hpRate == "25"_fx);
    CHECK(f.world.ecs().get<DropGoods>(child).m_epRate == "40"_fx);
    CHECK(f.world.ecs().get<RoleTimers>(child).summonMs == "100"_fx);
    CHECK(f.world.ecs().get<Transform>(child).position.x == f.world.ecs().get<Transform>(effect).position.x);
    CHECK(hit::standardHurt(f.world, child) == hit::standardHurt(f.world, f.owner));
    const auto old = hit::standardHurt(f.world, child);
    f.world.ecs().get<Attributes>(f.owner).basic[5] += "77"_fx;
    CHECK(hit::standardHurt(f.world, child) == old + "77"_fx);
    for (int i = 0; i < 6; ++i) f.tick();
    collect(); CHECK(count == 1);
    f.world.ecs().get<EntityGroup>(f.owner).suspended = true;
    for (int i = 0; i < 4; ++i) role::update(f.world, child);
    CHECK(f.world.ecs().get<Vitals>(child).hp == Fixed{});
    CHECK(f.world.ecs().get<RoleTimers>(child).summonMs == "-1"_fx);
}

TEST_CASE("Effect positions follow shared owner and action cleanup")
{
    Fixture f;
    f.row(1).follow = 1;
    f.row(2).positionType = 1;
    f.row(3).positionType = 0;
    f.row(3).relativePosition = {"0.5"_fx, "5"_fx, "0.5"_fx};
    f.load();
    f.world.ecs().singleton<MapRegion>().moveRanges.push_back({"10"_fx, "20"_fx, "100"_fx, "200"_fx});
    const auto zero = f.spawn();
    const auto follow = f.spawn(1);
    const auto absolute = f.spawn(2);
    const auto relative = f.spawn(3);
    CHECK(f.world.ecs().get<Transform>(zero).position.x == "90"_fx);
    CHECK(f.world.ecs().get<Transform>(zero).position.y == "25"_fx);
    CHECK(f.world.ecs().get<Transform>(zero).position.z == "52"_fx);
    CHECK(f.world.ecs().get<Transform>(absolute).position.x == "9"_fx);
    CHECK(f.world.ecs().get<Transform>(relative).position.x == "59"_fx);
    CHECK(f.world.ecs().get<Transform>(relative).position.z == "120"_fx);
    CHECK_FALSE(f.world.ecs().has<Attributes>(zero));
    CHECK(f.world.ecs().get<EffectState>(zero).m_owner == f.owner);
    f.world.ecs().get<Transform>(f.owner).position.x = "150"_fx;
    f.tick();
    CHECK(f.world.ecs().get<Transform>(follow).position.x == "140"_fx);
    effect::clearActionEffects(f.world, f.owner, {zero, follow});
    f.tick();
    CHECK_FALSE(f.world.ecs().alive(zero));
    CHECK(f.world.ecs().alive(follow));
    f.world.destroy(f.owner);
    f.tick();
    CHECK_FALSE(f.world.ecs().alive(follow));
    CHECK(f.world.ecs().alive(absolute));
}

TEST_CASE("Effect next lifecycle owner loss freeze and snapshot")
{
    Fixture f;
    f.row(0).nextEffectId = f.row(1).id;
    f.row(2).follow = 2;
    f.load();
    const auto first = f.spawn();
    const auto orphan = f.spawn(2);
    f.world.ecs().get<EffectState>(orphan).m_freezeMs = kLogicDtMs * "3"_fx;
    f.tick();
    const auto time = std::get<AttackEffect>(f.world.ecs().get<EffectTree>(orphan).m_instance.leaves[0]).m_action.m_time;
    CHECK(time == kLogicDtMs);
    f.tick();
    CHECK(std::get<AttackEffect>(f.world.ecs().get<EffectTree>(orphan).m_instance.leaves[0]).m_action.m_time == time);
    f.world.destroy(f.owner);
    effect::clearSkillEffectsByInterrupt(f.world, f.owner, 12);
    CHECK_FALSE(f.world.ecs().get<EntityGroup>(first).destroyRequested);
    effect::clearSkillEffectsByInterrupt(f.world, f.owner, 11);
    f.tick();
    CHECK_FALSE(f.world.ecs().alive(first));
    CHECK(f.world.ecs().alive(orphan));
    CHECK(f.world.ecs().get<Transform>(orphan).position.y == Fixed{});
    size_t count = 0;
    f.world.ecs().each<EffectState>([&](Entity, const EffectState& state) {
        if (state.m_configId == 200000002) { ++count; CHECK(state.m_owner == f.owner); }
    });
    CHECK(count == 1);
    auto clone = f.world.clone();
    ByteBuffer bytes;
    f.world.serialize(bytes);
    WorldHooks hooks;
    hooks.update[0] = WorldHooks::noEntityUpdate;
    World restored(hooks);
    REQUIRE(restored.deserialize(bytes));
    f.tick();
    FrameInput input; input.frame = f.world.ecs().singleton<WorldClock>().frame;
    clone.tick(input);
    restored.tick(input);
    CHECK(f.world.hash() == clone.hash());
    CHECK(f.world.hash() == restored.hash());
}

TEST_CASE("Effect child strict threshold scale and hit-count lifecycle")
{
    Fixture f;
    auto& action = f.archive.action_effect.back();
    action.effectIds = {f.row(1).id};
    action.effectFrames = {2};
    f.row(1).actionIds = {-1};
    conf::SkillHitConfig hit;
    hit.id = 200000001;
    hit.hitInterval = -1;
    hit.hitCounts = 1;
    f.archive.skill_hit.push_back(hit);
    f.row(2).hitId = hit.id;
    f.row(2).nextEffectId = f.row(1).id;
    f.row(3).hitId = hit.id;
    f.row(3).nextEffectId = f.row(1).id;
    f.load();
    f.world.ecs().get<RoleHitState>(f.owner).m_actionScale = "2"_fx;
    const auto parent = f.spawn();
    const auto interval = std::get<AttackEffect>(f.world.ecs().get<EffectTree>(parent).m_instance.leaves[0]).m_action.m_interval;
    CHECK(interval == "1000"_fx * kLogicDtSec / "2"_fx);
    f.world.ecs().singleton<WorldClock>().dtMs = interval * "2"_fx;
    effect::update(f.world, parent);
    CHECK(std::get<AttackEffect>(f.world.ecs().get<EffectTree>(parent).m_instance.leaves[0]).m_action.m_spawned[0] == 0);
    effect::update(f.world, parent);
    CHECK(std::get<AttackEffect>(f.world.ecs().get<EffectTree>(parent).m_instance.leaves[0]).m_action.m_spawned[0] == 1);
    const auto two = f.spawn(2);
    const auto three = f.spawn(3);
    f.world.ecs().get<EffectState>(two).m_hitTargets = {f.owner};
    f.world.ecs().get<EffectState>(three).m_hitTargets = {f.owner};
    effect::update(f.world, two);
    effect::update(f.world, three);
    CHECK(f.world.ecs().get<EntityGroup>(two).destroyRequested);
    CHECK(f.world.ecs().get<EntityGroup>(three).destroyRequested);
    size_t count = 0;
    f.world.ecs().each<EffectState>([&](Entity, const EffectState& state) { if (state.m_configId == 200000002) ++count; });
    CHECK(count == 2);
}
