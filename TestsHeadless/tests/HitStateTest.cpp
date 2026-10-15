#include <doctest/doctest.h>
#include "mugen/sim/role/Role.h"
#include "SkillTestSupport.h"
#include "mugen/sim/role/brain/RoleBrainDriver.h"
#include "mugen/sim/role/brain/states/HitStates.h"
#include "mugen/avatar/data/AvatarAssetCache.h"
#include <iostream>

using namespace mugen;
using namespace mugen::sim;

namespace
{
// 记录掉落接缝调用，不在测试中实现物品规则。
void recordDeathDrop(World& world, Entity entity)
{
    world.emit(entity, Tips{"death-drop-hook"});
}
// 通过表现测试记录器观察生命周期接缝的原始类名。
void recordBehavior(World& world, Entity entity, BFEvent event, const char* name)
{
    world.emit(entity, Tips{std::string(event == BFEvent::BehaviorStateStart ? "start:" : "end:") + name});
}
// 使用真实动画资源和完整世界更新推进受击叶子。
struct HitStateWorld
{
    World m_world;
    Entity m_entity;
    uint64_t m_frame = 0;
    // 创建有完整受击组件的角色。
    explicit HitStateWorld(WorldHooks hooks = {}) : m_world(hooks)
    {
        skill_test::loadConfig();
        auto* assets = AvatarAssetCache::getInstance();
        assets->addSearchPath("res_zhcn");
        REQUIRE(assets->load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/avatar.bin"));
        m_entity = skill_test::actor(m_world);
        auto& ecs = m_world.ecs();
        ecs.get<RoleIdentity>(m_entity).roleId = conf::Config::instance().table<conf::RoleConfig>().front().id;
        MotionState motion;
        for (const auto& [path, map] : assets->motionMaps)
        {
            bool complete = true;
            for (const auto* name : {"flyup", "flydown", "toground", "getup", "stand", "death", "hit", "hitswitch"})
            {
                const auto* animation = map.findMotion(name);
                if (!animation || animation->durationMs() <= 0) complete = false;
            }
            if (complete) { motion.motionFile = path; break; }
        }
        REQUIRE_FALSE(motion.motionFile.empty());
        ecs.add(m_entity, motion);
        ecs.add(m_entity, RoleHitState{});
        ecs.add(m_entity, RoleTimers{});
        ecs.add(m_entity, Displacement{});
        ecs.add(m_entity, ColliderState{});
        ecs.add(m_entity, ControlEventQueue{});
        ecs.add(m_entity, Attributes{});
        ecs.add(m_entity, RoleBrain{});
        ecs.add(m_entity, RoleControl{});
    }
    // 通过唯一帧入口运行完整角色管线。
    void tick()
    {
        FrameInput input;
        input.frame = ++m_frame;
        m_world.tick(input);
    }
    // 返回正在运行的叶子动作，而不是根据状态位猜测。
    behavior::ActionType active()
    {
        auto& brain = m_world.ecs().get<RoleBrain>(m_entity);
        if (brain.m_phase != role::brain::BrainPhase::Running)
            return behavior::ActionType::None;
        if (brain.m_branch == role::brain::RoleBranch::Death && brain.m_destroyStage == 1)
            return behavior::ActionType::Destroy;
        return role::brain::RoleBrainDriver(m_world, m_entity, brain, false).currentAction();
    }
};
// 直接驱动受击叶：构造临时视图并解包叶子状态。
void hitEnter(World& world, Entity entity, behavior::ActionType type, behavior::HitActionState& state)
{
    state = behavior::HitActionState{};
    role::brain::RoleContext rc{world, entity};
    role::brain::states::HitState::enter(rc, type, state);
}
BTStatus hitTick(World& world, Entity entity, behavior::ActionType type, behavior::HitActionState& state)
{
    role::brain::RoleContext rc{world, entity};
    return role::brain::states::HitState::update(rc, type, state);
}
void hitExit(World& world, Entity entity, behavior::ActionType type, behavior::HitActionState& state)
{
    role::brain::RoleContext rc{world, entity};
    role::brain::states::HitState::exit(rc, type, state);
}
}

// 浮空链必须依赖真实动画完成和位移落地，不能在测试中写完成标记。
TEST_CASE("HitState World launch recovery visits every distinct phase")
{
    HitStateWorld f;
    using A = behavior::ActionType;
    auto& ecs = f.m_world.ecs();
    ecs.get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::HitUp;
    ecs.get<Transform>(f.m_entity).position.y = "100"_fx;
    ecs.get<RoleHitState>(f.m_entity).forceVelocity.y = "0.5"_fx;
    ecs.get<RoleHitState>(f.m_entity).stiffTime = "150"_fx;
    std::vector<A> phases;
    std::map<A, int> counts;
    for (int i = 0; i < 400; ++i)
    {
        f.tick();
        const auto phase = f.active();
        ++counts[phase];
        if (phases.empty() || phases.back() != phase) phases.push_back(phase);
        if (phase == A::Idle) break;
    }
    CHECK(phases == std::vector<A>{A::HitUp, A::HitDown, A::HitFloor, A::GetUp, A::Wake, A::Idle});
    for (const auto phase : {A::HitUp, A::HitDown, A::HitFloor, A::GetUp, A::Wake})
    {
        CAPTURE(static_cast<int>(phase));
        CHECK(counts[phase] >= 1);
        const int expected = phase == A::HitUp ? 7 : phase == A::HitDown ? 11 : phase == A::HitFloor ? 5 : 1;
        CHECK(counts[phase] >= expected - 1);
        CHECK(counts[phase] <= expected + 1);
        std::cout << "HitState phase " << static_cast<int>(phase) << ": " << counts[phase] << " frames\n";
    }
    CHECK(std::find(ecs.get<RoleHitState>(f.m_entity).m_protectionBuffRequests.begin(),
        ecs.get<RoleHitState>(f.m_entity).m_protectionBuffRequests.end(), 201) !=
        ecs.get<RoleHitState>(f.m_entity).m_protectionBuffRequests.end());
}

// 死亡必须经过倒地完成、死亡动画和销毁叶子。
TEST_CASE("HitState World death on floor reaches destroy and both events")
{
    HitStateWorld f;
    auto& ecs = f.m_world.ecs();
    ecs.get<RoleIdentity>(f.m_entity).roleType = 2;
    ecs.get<Vitals>(f.m_entity).hp = {};
    ecs.get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::HitFloor;
    bool death = false;
    bool presentation = false;
    for (int i = 0; i < 400 && ecs.alive(f.m_entity); ++i)
    {
        f.tick();
        if (ecs.alive(f.m_entity)) death = death || f.active() == behavior::ActionType::Death;
        for (const auto& event : ecs.singleton<PresentationEvents>().events)
            presentation = presentation || std::holds_alternative<RoleDeath>(event.payload);
    }
    CHECK(death);
    CHECK_FALSE(ecs.alive(f.m_entity));
    CHECK(presentation);
    REQUIRE(ecs.singleton<sim::RoomState>().m_roleDeaths.size() == 1);
    CHECK(ecs.singleton<sim::RoomState>().m_roleDeaths.front() == f.m_entity);
}

// 真实死亡配置必须生成中立效果并调用一次物品接缝。
TEST_CASE("HitState configured death effect and drop hook execute")
{
    WorldHooks hooks;
    hooks.m_hit.m_dropGoods = recordDeathDrop;
    hooks.update[1] = WorldHooks::noEntityUpdate;
    HitStateWorld f(hooks);
    auto& ecs = f.m_world.ecs();
    const auto& roles = conf::Config::instance().table<conf::RoleConfig>();
    const auto found = std::find_if(roles.begin(), roles.end(), [](const auto& row) {
        return row.deathEffectId != -1 && conf::Config::instance().find<conf::EffectConfig>(row.deathEffectId);
    });
    REQUIRE(found != roles.end());
    const auto effectId = found->deathEffectId;
    ecs.get<RoleIdentity>(f.m_entity).roleId = found->id;
    ecs.get<RoleIdentity>(f.m_entity).roleType = 2;
    ecs.get<Vitals>(f.m_entity).hp = {};
    ecs.get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Death;
    ecs.get<RoleStatus>(f.m_entity).entityParams.death_displacement_id = -1;
    int drops = 0;
    bool spawned = false;
    for (int i = 0; i < 100 && ecs.alive(f.m_entity); ++i)
    {
        f.tick();
        for (const auto& event : ecs.singleton<PresentationEvents>().events)
            if (const auto* tip = std::get_if<Tips>(&event.payload); tip && tip->text == "death-drop-hook") ++drops;
        ecs.each<EffectState>([&](Entity entity, const EffectState& effect) {
            if (effect.m_owner == f.m_entity && effect.m_configId == effectId)
            {
                spawned = true;
                CHECK(ecs.get<RoleIdentity>(entity).camp == 1);
                CHECK(effect.m_hitEntity == f.m_entity);
            }
        });
    }
    CHECK(spawned);
    CHECK(drops == 1);
    CHECK_FALSE(ecs.alive(f.m_entity));
}

// 各追击类型必须读取各自的位移字段并产生不同后继。
TEST_CASE("HitState chase hit types preserve distinct transitions")
{
    HitStateWorld f;
    using A = behavior::ActionType;
    auto& ecs = f.m_world.ecs();
    for (int type = 0; type <= 2; ++type)
    {
        const auto& rows = conf::Config::instance().table<conf::SkillHitConfig>();
        const auto found = std::find_if(rows.begin(), rows.end(), [type](const auto& row) {
            return row.hitType == type && row.id != 0 && row.displacementId != -1;
        });
        REQUIRE(found != rows.end());
            behavior::HitActionState state;
        hitEnter(f.m_world, f.m_entity, A::HitUp, state);
        ecs.get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::HitUp;
        ecs.get<RoleStatus>(f.m_entity).entityParams.hit_id = found->id;
        ecs.get<RoleHitState>(f.m_entity).rigidity = "10000"_fx;
        const auto result = hitTick(f.m_world, f.m_entity, A::HitUp, state);
        CHECK(state.m_displacementId ==
            (type == 0 ? found->airDisplacementId : found->displacementId));
        CHECK_FALSE(ecs.get<RoleStatus>(f.m_entity).entityParams.hit_id.has_value());
        CHECK((result == BTStatus::Success) == (type == 1));
        CHECK(ecs.get<RoleStatus>(f.m_entity).isHitDown() == (type == 1));
    }
}

// 下落和倒地追击不能把击倒与再次挑起合并为同一转换。
TEST_CASE("HitState falling and floor chase matrix retains separate transitions")
{
    HitStateWorld f;
    auto& ecs = f.m_world.ecs();
    using A = behavior::ActionType;
    using S = EntityRoleStatus;
    for (const auto action : {A::HitDown, A::HitFloor})
        for (int type = 0; type <= 2; ++type)
        {
            CAPTURE(static_cast<int>(action));
            CAPTURE(type);
            const auto& rows = conf::Config::instance().table<conf::SkillHitConfig>();
            const auto found = std::find_if(rows.begin(), rows.end(), [type](const auto& row) {
                return row.hitType == type && row.displacementId != -1;
            });
            REQUIRE(found != rows.end());
                    behavior::HitActionState state;
            hitEnter(f.m_world, f.m_entity, action, state);
            ecs.get<RoleStatus>(f.m_entity).eEntityStatus = S::Idle | (action == A::HitDown ? S::HitDown : S::HitFloor);
            ecs.get<RoleStatus>(f.m_entity).entityParams.hit_id = found->id;
            ecs.get<RoleHitState>(f.m_entity).rigidity = "10000"_fx;
            const auto result = hitTick(f.m_world, f.m_entity, action, state);
            CHECK(state.m_displacementId ==
                (type == 0 ? found->airDisplacementId : found->displacementId));
            CHECK(ecs.get<RoleStatus>(f.m_entity).isHitUp() == (type != 1));
            CHECK((result == BTStatus::Success) == (action == A::HitFloor || type != 1));
            if (action == A::HitFloor && type == 1)
                CHECK(ecs.get<RoleStatus>(f.m_entity).isHitDown());
        }
}

// 保护耗尽后逐次施加重量，并从同一组件读取疲劳和连击数。
TEST_CASE("HitState exhausted rigidity accelerates falling and reduces stun")
{
    HitStateWorld f;
    auto& ecs = f.m_world.ecs();
    const auto& rows = conf::Config::instance().table<conf::RoleConfig>();
    const auto found = std::find_if(rows.begin(), rows.end(), [](const auto& row) { return row.rigidity > 0; });
    REQUIRE(found != rows.end());
    ecs.get<RoleIdentity>(f.m_entity).roleId = found->id;
    auto& state = ecs.get<RoleHitState>(f.m_entity);
    state.rigidity = "1"_fx;
    state.weight = "0.1"_fx;
    state.hitCounts = 2;
    state.fatigue = "10"_fx;
    state.stiffTime = "100"_fx;
    CHECK(Role(f.m_world, f.m_entity).applyWeight("2"_fx));
    CHECK(state.forceVelocity.y == -state.weight * "2"_fx);
    CHECK(Role(f.m_world, f.m_entity).getStiffTime() == "80"_fx);
    const auto velocity = state.forceVelocity.y;
    CHECK(Role(f.m_world, f.m_entity).applyWeight("1"_fx));
    CHECK(state.forceVelocity.y < velocity);
    CHECK(Role(f.m_world, f.m_entity).getStiffTime() == "60"_fx);
}

// 在受击叶子运行期间通过技能施放入口选择挣脱槽。
TEST_CASE("HitState breakaway cast exits hit leaf into Attack")
{
    HitStateWorld f;
    auto& ecs = f.m_world.ecs();
    int32_t id = -1;
    std::string attackMotion;
    for (const auto& skill : conf::Config::instance().table<conf::SkillAttackConfig>())
    {
        if (skill.cd != 0 || skill.actionIds.empty() || skill.actionIds.front().empty()) continue;
        const auto* action = conf::Config::instance().find<conf::ActionConfig>(skill.actionIds.front().front());
        if (!action) continue;
        const auto name = std::holds_alternative<std::string>(action->action) ? std::get<std::string>(action->action) :
            std::to_string(std::get<int32_t>(action->action));
        for (const auto& [path, map] : AvatarAssetCache::getInstance()->motionMaps)
        {
            if (path.find("/hero/") == std::string::npos || !map.findMotion("flyup")) continue;
            const auto* motion = map.findMotion(name);
            if (motion && motion->durationMs() >= 300) { id = skill.id; attackMotion = path; break; }
        }
        if (id != -1) break;
    }
    REQUIRE(id != -1);
    ecs.get<MotionState>(f.m_entity).motionFile = attackMotion;
    skill::SkillLoadout loadout(5);
    loadout[0].resize(12);
    loadout[0][11] = {{{id, -1}}};
    skill::SkillCaster(f.m_world, f.m_entity).loadSkill(loadout);
    ecs.get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::HitUp;
    ecs.get<Transform>(f.m_entity).position.y = "100"_fx;
    f.tick();
    REQUIRE(f.active() == behavior::ActionType::HitUp);
    const auto index = skill::getGivenSkill(ecs.get<SkillPool>(f.m_entity), "L");
    REQUIRE(index != kNoSkill);
    REQUIRE(skill::SkillCaster(f.m_world, f.m_entity).presetSkill("L"));
    for (int i = 0; i < 4 && !ecs.get<RoleStatus>(f.m_entity).isAttack(); ++i) f.tick();
    CHECK(ecs.get<RoleStatus>(f.m_entity).isAttack());
    CHECK(ecs.get<RoleHitState>(f.m_entity).stiffTime == Fixed{});
    bool attackRunning = false;
    for (int frame = 0; frame < 4 && !attackRunning; ++frame)
    {
        attackRunning = ecs.get<RoleBrain>(f.m_entity).m_branch == role::brain::RoleBranch::Attack;
        if (!attackRunning) f.tick();
    }
    CHECK(attackRunning);
}

// 麻痹保持原动画，眩晕优先启动受控动作。
TEST_CASE("HitState Controlled preserves Palsy animation unless Stun is active")
{
    HitStateWorld f;
    auto& ecs = f.m_world.ecs();
    ecs.get<MotionState>(f.m_entity).motionName = "stand";
    ecs.get<sim::ExtraState>(f.m_entity).add(mugen::ExtraState::Palsy);
    behavior::HitActionState leaf;
    hitEnter(f.m_world, f.m_entity, behavior::ActionType::Controlled, leaf);
    CHECK(ecs.get<MotionState>(f.m_entity).motionName == "stand");
    ecs.get<sim::ExtraState>(f.m_entity).add(mugen::ExtraState::Stun);
    hitEnter(f.m_world, f.m_entity, behavior::ActionType::Controlled, leaf);
    CHECK(ecs.get<MotionState>(f.m_entity).motionName == "hit");
    const auto& events = ecs.singleton<PresentationEvents>().events;
    REQUIRE_FALSE(events.empty());
    REQUIRE(std::holds_alternative<RoleIntensity>(events.back().payload));
    CHECK(std::get<RoleIntensity>(events.back().payload).m_value == "1.3"_fx);
}

// 竞技保护按血量阈值触发，刷新和超时保持各自边界。
TEST_CASE("HitState PVP protection threshold refresh and expiry")
{
    World world;
    WorldSetup setup;
    setup.m_pvp = true;
    world.init(setup);
    const auto entity = world.create(GroupKind::Role);
    Vitals vitals;
    vitals.hp = vitals.maxHp = "1000"_fx;
    world.ecs().add(entity, vitals);
    world.ecs().add(entity, RoleHitState{});
    world.ecs().add(entity, RoleStatus{});
    Role(world, entity).initHitProtect();
    auto& state = world.ecs().get<RoleHitState>(entity);
    REQUIRE(state.m_hitProtectHp.has_value());
    const auto threshold = *state.m_hitProtectHp;
    CHECK(threshold > "549"_fx);
    CHECK(threshold < "551"_fx);
    const auto initialTime = state.m_hitProtectTime;
    Role(world, entity).updateHitProtect();
    CHECK(state.m_hitProtectTime < initialTime);
    Role(world, entity).refreshHitProtect();
    CHECK(state.m_hitProtectTime == initialTime);
    world.ecs().get<Vitals>(entity).hp = threshold;
    Role(world, entity).updateHitProtect();
    CHECK(state.m_hitProtectExecuted);
    const auto executedTime = state.m_hitProtectTime;
    Role(world, entity).refreshHitProtect();
    CHECK(state.m_hitProtectTime == executedTime);
    Role(world, entity).resetHitProtect();
    CHECK_FALSE(state.m_hitProtectHp.has_value());
    CHECK_FALSE(state.m_hitProtectExecuted);
    Role(world, entity).initHitProtect();
    state.m_hitProtectTime = world.ecs().singleton<WorldClock>().dtMs;
    Role(world, entity).updateHitProtect();
    CHECK_FALSE(state.m_hitProtectHp.has_value());
}

// 复活入口恢复生命并依状态守卫回到待机。
TEST_CASE("HitState Revive restores HP and returns through guard exit")
{
    HitStateWorld f;
    auto& ecs = f.m_world.ecs();
    ecs.get<Vitals>(f.m_entity).hp = {};
    ecs.get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Revive;
    f.tick();
    CHECK(ecs.get<Vitals>(f.m_entity).hp == ecs.get<Vitals>(f.m_entity).maxHp);
    f.tick();
    CHECK(f.active() == behavior::ActionType::Idle);
}

// 开始和结束通知必须保留动作类名，不能退化为动画名称。
TEST_CASE("HitState lifecycle seam preserves action class names")
{
    WorldHooks hooks;
    hooks.m_hit.m_behaviorState = recordBehavior;
    HitStateWorld f(hooks);
    behavior::HitActionState leaf;
    hitEnter(f.m_world, f.m_entity, behavior::ActionType::Wake, leaf);
    hitExit(f.m_world, f.m_entity, behavior::ActionType::Wake, leaf);
    std::vector<std::string> names;
    for (const auto& event : f.m_world.ecs().singleton<PresentationEvents>().events)
        if (const auto* tip = std::get_if<Tips>(&event.payload)) names.push_back(tip->text);
    CHECK(names == std::vector<std::string>{"start:ActionWake", "end:ActionWake"});
}

// 空中恢复途中快照必须保留叶子、可选打击和计时数据。
TEST_CASE("HitState snapshot resumes airborne recovery deterministically")
{
    HitStateWorld f;
    auto& ecs = f.m_world.ecs();
    ecs.get<RoleStatus>(f.m_entity).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::HitUp;
    ecs.get<Transform>(f.m_entity).position.y = "100"_fx;
    ecs.get<RoleHitState>(f.m_entity).forceVelocity.y = "0.5"_fx;
    for (int i = 0; i < 3; ++i) f.tick();
    ByteBuffer bytes;
    f.m_world.serialize(bytes);
    bytes.readFinish();
    World restored;
    REQUIRE(restored.deserialize(bytes));
    CHECK(restored.hash() == f.m_world.hash());
    for (int i = 0; i < 35; ++i)
    {
        f.tick();
        FrameInput input;
        input.frame = f.m_frame;
        restored.tick(input);
        CHECK(restored.hash() == f.m_world.hash());
    }
}

// 正配置总值耗尽才进入保护，零配置不表示永久保护。
TEST_CASE("HitState rigidity requires positive configured capacity")
{
    RoleHitState state;
    CHECK_FALSE(state.isRigidity(Fixed{}));
    CHECK_FALSE(state.isRigidity("-1"_fx));
    CHECK(state.isRigidity("10"_fx));
    state.rigidity = "1"_fx;
    CHECK_FALSE(state.isRigidity("10"_fx));
    state.rigidity = "-1"_fx;
    CHECK(state.isRigidity("10"_fx));
}

// 重复读取会持续扣减存储值，且不会减为负数。
TEST_CASE("HitState fatigue mutates stiffness on every eligible read")
{
    RoleHitState state;
    state.stiffTime = "100"_fx;
    state.fatigue = "15"_fx;
    state.hitCounts = 2;
    CHECK(state.getStiffTime("10"_fx, false) == "70"_fx);
    CHECK(state.stiffTime == "70"_fx);
    CHECK(state.getStiffTime("10"_fx, false) == "40"_fx);
    CHECK(state.getStiffTime("10"_fx, false) == "10"_fx);
    CHECK(state.getStiffTime("10"_fx, false) == Fixed{});
    CHECK(state.getStiffTime("10"_fx, false) == Fixed{});
}

// 死亡、未耗尽和零配置三条路径都不能扣减硬直。
TEST_CASE("HitState stiffness preserves death and unprotected paths")
{
    RoleHitState state;
    state.stiffTime = "100"_fx;
    state.fatigue = "15"_fx;
    state.hitCounts = 2;
    CHECK(state.getStiffTime("10"_fx, true) == "100"_fx);
    CHECK(state.getStiffTime(Fixed{}, false) == "100"_fx);
    state.rigidity = "1"_fx;
    CHECK(state.getStiffTime("10"_fx, false) == "100"_fx);
    state.rigidity = {};
    state.hitCounts = 0;
    CHECK(state.getStiffTime("10"_fx, false) == "100"_fx);
}
