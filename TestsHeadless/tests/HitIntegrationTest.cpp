#include "SkillTestSupport.h"
#include "mugen/sim/hit/Hit.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include <iostream>

namespace
{
using namespace skill_test;
// 以世界表现队列记录接缝调用，不使用全局可变观测器。
void observe(World& world, Entity entity, BFEvent event, const HitTrigger& data)
{
    world.emit(entity, Tips{"BF:" + std::to_string(static_cast<int>(event))});
    if (event == BFEvent::BeforeDeath)
    {
        CHECK_FALSE(status::isDeathStatus(world.ecs().get<RoleStatus>(entity)));
        CHECK(world.ecs().get<Vitals>(entity).hp == Fixed{});
    }
    if (event == BFEvent::BeforeHit && data.m_teammate) CHECK(data.m_target.value != 0);
}
// 测试免死 Buff 接缝，同步恢复生命而不实现 Buff 规则。
void rescue(World& world, Entity entity, BFEvent event, const HitTrigger& data)
{
    observe(world, entity, event, data);
    if (event == BFEvent::BeforeDeath) world.ecs().get<Vitals>(entity).hp = "7"_fx;
}
// 记录原始 Buff 施加来源，施加目标必须为空。
void applied(World& world, Entity entity, const HitBuffApplication& data)
{
    CHECK(data.m_source.m_target.value == 0);
    world.emit(entity, Tips{"ADD:" + std::to_string(data.m_buffId.toInt())});
}
// 寒气查询替身只提供属性，不执行 Buff 生命周期。
HpBuffModifiers chilled(World&, Entity) { return {true, {}}; }
// 治疗变化查询替身。
HpBuffModifiers healing(World&, Entity) { return {false, "0.5"_fx}; }
// 最后一层护盾消耗查询替身。
bool lastShield(World&, Entity) { return true; }
// 测试场景使用真实配置并只覆写一组可控的效果和打击行。
struct HitFixture
{
    // 场景配置副本。
    conf::ConfigArchive m_archive;
    // 独立世界及测试角色。
    World m_world;
    Entity m_hero, m_monster, m_effect;
    // 真实效果行与打击行的配置标识。
    int32_t m_effectId = 920000, m_hitId = 920000;
    // 构造可通过实际 World 碰撞阶段命中的场景。
    explicit HitFixture(HitHooks hooks = {})
    {
        loadConfig();
#define COPY_HIT_TABLE(kind, Type, Key) m_archive.kind = conf::Config::instance().table<conf::Type>();
        MG_CONFIG_TABLES(COPY_HIT_TABLE)
#undef COPY_HIT_TABLE
        WorldHooks worldHooks;
        worldHooks.m_hit = hooks;
        worldHooks.update[0] = WorldHooks::noEntityUpdate;
        worldHooks.update[1] = WorldHooks::noEntityUpdate;
        m_world = World(worldHooks);
        m_world.init({});
        REQUIRE(conf::Config::instance().find<conf::EffectConfig>(m_effectId));
        m_hitId = effectRow().hitId;
        auto& h = hitRow();
        h.hitInterval = -1; h.hitCounts = -1; h.hitMust = 1;
        h.hitType = 0; h.hitCondition = -1;
        h.freezeTime = 100; h.freezeTimeDelay = 50;
        h.freezeTimeControlEffect = 0; h.freezeTimeControlRole = 0;
        auto& e = effectRow();
        e.hitTarget = 0; e.hitEffectIds = {-1}; e.debuffId = {-1};
        e.buffId = {"-1"_fx}; e.buffAllId = -1; e.hitExtraControl = {0,0};
        e.specialabilityId = -1; e.specialabilityAllId = -1;
        load();
        m_hero = role(1, 2);
        m_monster = role(2, 8);
        m_effect = effect::create(m_world, m_hero, m_effectId);
        REQUIRE(m_effect.value != 0);
        m_world.ecs().get<ColliderState>(m_effect).attack = {box()};
        m_world.ecs().get<EffectState>(m_effect).m_skillId = 920000;
        m_world.ecs().get<EffectState>(m_effect).m_baseSkillId = 920000;
    }
    // 恢复全局只读配置，防止污染相邻测试。
    ~HitFixture() { conf::Config::instance().load(std::string(MUGEN_CONTENT_DIR) + "/mugen/config/config.bin"); }
    // 标准碰撞框，位于地面。
    static WorldBox box() { return {{}, {"20"_fx, {}, "20"_fx}}; }
    // 获取将被覆写的效果配置。
    conf::EffectConfig& effectRow()
    {
        auto it = std::find_if(m_archive.effect.begin(), m_archive.effect.end(), [&](const auto& row) { return row.id == m_effectId; });
        REQUIRE(it != m_archive.effect.end());
        return *it;
    }
    // 获取将被覆写的打击配置。
    conf::SkillHitConfig& hitRow()
    {
        auto it = std::find_if(m_archive.skill_hit.begin(), m_archive.skill_hit.end(), [&](const auto& row) { return row.id == m_hitId; });
        REQUIRE(it != m_archive.skill_hit.end());
        return *it;
    }
    // 严格序列化完整配置归档后重新加载。
    void load()
    {
        ByteBuffer bytes;
        for (const auto c : conf::kConfigMagic) bytes.writeByte(c);
        bytes.writeUint32(conf::kConfigVersion); bytes.writeUint64(conf::configSchemaHash());
        BinaryWriter(bytes).write(m_archive); bytes.writeFinish();
        REQUIRE(conf::Config::instance().loadBytes({bytes.getBuffer(), bytes.getBuffer() + bytes.getSize()}));
    }
    // 从真实角色行生成拥有完整受击组件的测试实体。
    Entity role(int type, int camp)
    {
        const auto& rows = conf::Config::instance().table<conf::RoleConfig>();
        auto it = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.roleType == type && row.hitStiffTime == -1; });
        REQUIRE(it != rows.end());
        auto& ecs = m_world.ecs();
        const auto entity = m_world.create(GroupKind::Role);
        RoleIdentity identity;
        identity.roleId = it->id; identity.roleType = type; identity.camp = camp; identity.hostileCamp = camp == 2 ? 25 : 7;
        ecs.add(entity, identity); ecs.add(entity, RoleStatus{}); ecs.add(entity, sim::ExtraState{});
        ecs.add(entity, Transform{}); ecs.add(entity, RoleTimers{}); ecs.add(entity, RoleHitState{});
        ecs.add(entity, Displacement{}); ecs.add(entity, MotionState{});
        Attributes attributes; attributes.basic[4] = "100000"_fx; attributes.basic[5] = "800"_fx;
        attributes.basic[6] = "800"_fx; attributes.basic[7] = "400"_fx; attributes.basic[8] = "200"_fx;
        ecs.add(entity, attributes);
        Vitals vitals; vitals.hp = vitals.maxHp = "100000"_fx;
        ecs.add(entity, vitals);
        ColliderState collider; collider.radius = "20"_fx;
        if (camp == 8) collider.damage = {box()};
        ecs.add(entity, collider);
        return entity;
    }
    // 推进真正的 World 碰撞和定时器阶段。
    void tick() { FrameInput input; input.frame = m_world.ecs().singleton<WorldClock>().frame + 1; m_world.tick(input); }
    // 用同一世界的 RNG 克隆预计算期望伤害，不改变生产 RNG。
    Fixed expected(Entity target)
    {
        auto clone = m_world.clone();
        hit::DamageInput input;
        REQUIRE(hit::damageInput(clone, m_effect, target, input));
        auto draw = hit::damageDraw(clone.ecs().singleton<RngState>().random);
        return hit::attributeDodge(input, draw) ? Fixed{} : hit::calculateDamage(input, draw).m_damage;
    }
};
// 查询同步事件的顺序与次数。
std::vector<std::string> messages(World& world, Entity entity)
{
    std::vector<std::string> result;
    for (const auto& event : world.ecs().singleton<PresentationEvents>().events)
        if (event.entity == entity) if (const auto* tip = std::get_if<Tips>(&event.payload)) result.push_back(tip->text);
    return result;
}
}

// 真实碰撞阶段连接伤害、受击状态、表现与单目标去重。
// 醒来状态拒绝碰撞伤害，即使配置要求必中也不能覆盖状态保护。
TEST_CASE("HitState Wake ignores actual collision damage")
{
    HitFixture f;
    auto& ecs = f.m_world.ecs();
    ecs.get<RoleStatus>(f.m_monster).eEntityStatus = EntityRoleStatus::Idle | EntityRoleStatus::Wake;
    const auto hp = ecs.get<Vitals>(f.m_monster).hp;
    f.tick();
    CHECK(ecs.get<Vitals>(f.m_monster).hp == hp);
    CHECK(status::isWakeStatus(ecs.get<RoleStatus>(f.m_monster)));
    CHECK_FALSE(status::isHitStatus(ecs.get<RoleStatus>(f.m_monster)));
}

TEST_CASE("Hit collision hero monster and per target dedup")
{
    HitFixture f;
    auto& ecs = f.m_world.ecs();
    const auto expected = f.expected(f.m_monster);
    REQUIRE(expected > Fixed{});
    f.tick();
    CHECK(ecs.get<Vitals>(f.m_monster).hp == "100000"_fx - expected);
    CHECK(status::isHitStatus(ecs.get<RoleStatus>(f.m_monster)));
    CHECK(ecs.get<RoleStatus>(f.m_monster).entityParams.hit_id == f.m_hitId);
    CHECK(ecs.get<EffectState>(f.m_effect).m_hitTargets == std::vector<Entity>{f.m_monster});
    bool shown = false;
    for (const auto& event : ecs.singleton<PresentationEvents>().events)
        if (const auto* number = std::get_if<HurtNumber>(&event.payload)) shown |= event.entity == f.m_monster && number->value == expected.toInt();
    CHECK(shown);
    f.tick();
    CHECK(ecs.get<Vitals>(f.m_monster).hp == "100000"_fx - expected);
    std::cout << "Hit QA: hero -> monster damage=" << expected.toInt() << " Hit=1 dedup=1\n";
}

// 无打击配置、结束接触与孤立特效不消耗伤害随机值。
TEST_CASE("Hit minus one and orphan failure paths")
{
    HitFixture f;
    f.effectRow().hitId = -1; f.load();
    auto expectedWorld = f.m_world.clone();
    f.tick();
    CHECK(f.m_world.ecs().get<Vitals>(f.m_monster).hp == "100000"_fx);
    CHECK_FALSE(status::isHitStatus(f.m_world.ecs().get<RoleStatus>(f.m_monster)));
    CHECK(f.m_world.ecs().singleton<RngState>().random.next() == expectedWorld.ecs().singleton<RngState>().random.next());
    f.effectRow().hitId = f.m_hitId; f.load();
    f.m_world.ecs().get<EffectState>(f.m_effect).m_owner = {};
    f.tick();
    CHECK(f.m_world.ecs().get<Vitals>(f.m_monster).hp == "100000"_fx);
    std::cout << "Hit QA: hit_id=-1 hp_unchanged=1\n";
}

// 正间隔不在首次命中时立即重置，因此同一帧可打多个目标。
TEST_CASE("Hit positive interval multi target and count")
{
    HitFixture f;
    f.hitRow().hitInterval = 200; f.hitRow().hitCounts = 2; f.load();
    const auto second = f.role(2,8), third = f.role(2,8);
    f.tick();
    auto& ecs = f.m_world.ecs();
    CHECK(ecs.get<Vitals>(f.m_monster).hp < "100000"_fx);
    CHECK(ecs.get<Vitals>(second).hp < "100000"_fx);
    CHECK(ecs.get<Vitals>(third).hp == "100000"_fx);
    CHECK(ecs.get<EffectState>(f.m_effect).m_hitCount == 2);
    CHECK(ecs.get<EffectState>(f.m_effect).m_hitInterval == Fixed{});
    effect::enter(f.m_world, f.m_effect);
    effect::update(f.m_world, f.m_effect);
    CHECK(ecs.get<EffectState>(f.m_effect).m_hitCount == 0);
    CHECK(ecs.get<EffectState>(f.m_effect).m_hitInterval > Fixed{});
    const auto hp = ecs.get<Vitals>(f.m_monster).hp;
    hit::onEffectCollision(f.m_world, f.m_effect, f.m_monster, CollisionEvent::Continue, {});
    CHECK(ecs.get<Vitals>(f.m_monster).hp == hp);
}

// 覆盖严格护盾比较、倍率溢出、锁血和跳过护盾。
TEST_CASE("Hp shield absorption break equality scale and lock")
{
    HitHooks hooks; hooks.m_trigger = observe;
    HitFixture f(hooks); auto& ecs = f.m_world.ecs();
    auto& v = ecs.get<Vitals>(f.m_monster); v.hp = "100"_fx; v.m_shield = "50"_fx;
    hit::modifyHp(f.m_world,f.m_monster,"-20"_fx);
    CHECK(v.hp == "100"_fx); CHECK(v.m_shield == "30"_fx);
    hit::modifyHp(f.m_world,f.m_monster,"-40"_fx);
    CHECK(v.hp == "90"_fx); CHECK(v.m_shield == Fixed{});
    v.m_shield = "10"_fx;
    hit::modifyHp(f.m_world,f.m_monster,"-10"_fx);
    CHECK(v.hp == "90"_fx); CHECK(v.m_shield == Fixed{});
    v.m_shield = "50"_fx; v.m_shieldConsumeScale = "2"_fx;
    hit::modifyHp(f.m_world,f.m_monster,"-30"_fx);
    CHECK(v.hp == "90"_fx); CHECK(v.m_shield == Fixed{});
    v.m_hpLock = "3"_fx;
    hit::modifyHp(f.m_world,f.m_monster,"-1000"_fx);
    CHECK(v.hp == "3"_fx); CHECK_FALSE(hit::isDead(f.m_world,f.m_monster));
    v.m_shield = "50"_fx; v.m_hpLock = {};
    hit::modifyHp(f.m_world,f.m_monster,"-1"_fx,{},true);
    CHECK(v.hp == "2"_fx); CHECK(v.m_shield == "50"_fx);
}

// 免死回调先于死亡入口执行，回调不恢复时进入 Death。
TEST_CASE("Hp BEFORE_DEATH ordering rescue and death")
{
    SUBCASE("death")
    {
        HitHooks hooks; hooks.m_trigger = observe;
        HitFixture f(hooks);
        hit::modifyHp(f.m_world,f.m_monster,"-200000"_fx);
        CHECK(status::isDeathStatus(f.m_world.ecs().get<RoleStatus>(f.m_monster)));
        CHECK(messages(f.m_world,f.m_monster) == std::vector<std::string>{"BF:17", "BF:14"});
    }
    SUBCASE("rescue")
    {
        HitHooks hooks; hooks.m_trigger = rescue;
        HitFixture f(hooks);
        hit::modifyHp(f.m_world,f.m_monster,"-200000"_fx);
        CHECK(f.m_world.ecs().get<Vitals>(f.m_monster).hp == "7"_fx);
        CHECK_FALSE(status::isDeathStatus(f.m_world.ecs().get<RoleStatus>(f.m_monster)));
        CHECK(messages(f.m_world,f.m_monster) == std::vector<std::string>{"BF:17", "BF:14", "BF:21"});
    }
}

// 治疗与按层护盾行为通过明确接缝提供输入。
TEST_CASE("Hp chill healing change and shield layer seams")
{
    SUBCASE("chill")
    {
        HitHooks hooks; hooks.m_modifiers = chilled; HitFixture f(hooks);
        f.m_world.ecs().get<Vitals>(f.m_monster).hp = "100"_fx;
        hit::modifyHp(f.m_world,f.m_monster,"20"_fx);
        CHECK(f.m_world.ecs().get<Vitals>(f.m_monster).hp == "100"_fx);
    }
    SUBCASE("healing")
    {
        HitHooks hooks; hooks.m_modifiers = healing; HitFixture f(hooks);
        f.m_world.ecs().get<Vitals>(f.m_monster).hp = "100"_fx;
        hit::modifyHp(f.m_world,f.m_monster,"20"_fx);
        CHECK(f.m_world.ecs().get<Vitals>(f.m_monster).hp == "130"_fx);
    }
    SUBCASE("last shield layer")
    {
        HitHooks hooks; hooks.m_shieldLayer = lastShield; HitFixture f(hooks);
        f.m_world.ecs().get<Vitals>(f.m_monster).m_shield = "100"_fx;
        hit::modifyHp(f.m_world,f.m_monster,"-1"_fx);
        CHECK(f.m_world.ecs().get<Vitals>(f.m_monster).m_shield == Fixed{});
        CHECK(f.m_world.ecs().get<Vitals>(f.m_monster).hp == "100000"_fx);
    }
}

// 霸体只受伤，不修改冻结、朝向或硬直。
TEST_CASE("Hit super armor has no stun freeze or facing change")
{
    HitFixture f; auto& ecs = f.m_world.ecs();
    ecs.get<sim::ExtraState>(f.m_monster).add(mugen::ExtraState::SuperArmor);
    f.tick();
    CHECK(ecs.get<Vitals>(f.m_monster).hp < "100000"_fx);
    CHECK_FALSE(status::isHitStatus(ecs.get<RoleStatus>(f.m_monster)));
    CHECK(ecs.get<RoleHitState>(f.m_monster).stiffTime == Fixed{});
    CHECK(ecs.get<Transform>(f.m_monster).towardX == 1);
    CHECK(ecs.get<RoleTimers>(f.m_hero).freezeMs == Fixed{});
    CHECK(ecs.get<EffectState>(f.m_effect).m_freezeMs == Fixed{});
    CHECK(ecs.singleton<LogicTimers>().entries.empty());
}

// 真正定时器阶段推进延迟，快照恢复后得到相同冻结与哈希。
TEST_CASE("Hit delayed freeze timers replacement and snapshot")
{
    HitFixture f; auto& ecs = f.m_world.ecs();
    f.tick();
    CHECK(ecs.get<RoleTimers>(f.m_hero).freezeMs == "100"_fx);
    CHECK(ecs.get<EffectState>(f.m_effect).m_freezeMs == "100"_fx);
    CHECK(ecs.get<RoleTimers>(f.m_monster).freezeMs == Fixed{});
    REQUIRE(ecs.singleton<LogicTimers>().entries.size() == 1);
    CHECK(ecs.singleton<LogicTimers>().entries[0].intervalMs == "50"_fx);
    f.tick();
    CHECK(ecs.get<RoleTimers>(f.m_monster).freezeMs == Fixed{});
    ByteBuffer bytes; f.m_world.serialize(bytes); bytes.readFinish();
    WorldHooks hooks; hooks.update[0] = WorldHooks::noEntityUpdate; hooks.update[1] = WorldHooks::noEntityUpdate;
    World restored(hooks); REQUIRE(restored.deserialize(bytes));
    f.tick(); FrameInput input; input.frame = 3; restored.tick(input);
    CHECK(ecs.get<RoleTimers>(f.m_monster).freezeMs == "100"_fx);
    CHECK(ecs.get<RoleHitState>(f.m_monster).m_freezeTimer == 0);
    CHECK(restored.hash() == f.m_world.hash());
    f.hitRow().hitInterval = 0; f.load();
    hit::onEffectCollision(f.m_world,f.m_effect,f.m_monster,CollisionEvent::Continue,{});
    const auto old = ecs.get<RoleHitState>(f.m_monster).m_freezeTimer;
    hit::onEffectCollision(f.m_world,f.m_effect,f.m_monster,CollisionEvent::Continue,{});
    CHECK(ecs.get<RoleHitState>(f.m_monster).m_freezeTimer != old);
    CHECK(ecs.singleton<LogicTimers>().entries.size() == 1);
}

// 同阵营 Buff 仍施加到主人池，友方事件发给碰撞对象。
TEST_CASE("Hit same camp buff payload and enemy debuff")
{
    HitHooks hooks; hooks.m_trigger = observe; hooks.m_apply = applied;
    HitFixture f(hooks);
    f.effectRow().hitTarget = -1; f.effectRow().buffAllId = std::vector<int32_t>{123};
    f.effectRow().buffId = {"456"_fx}; f.effectRow().debuffId = {789}; f.load();
    const auto ally = f.role(1,2);
    hit::onEffectCollision(f.m_world,f.m_effect,ally,CollisionEvent::Began,{});
    CHECK(messages(f.m_world,f.m_hero) == std::vector<std::string>{"ADD:123"});
    CHECK(messages(f.m_world,ally) == std::vector<std::string>{"BF:3","BF:4"});
    hit::onEffectCollision(f.m_world,f.m_effect,f.m_hero,CollisionEvent::Began,{});
    CHECK(messages(f.m_world,f.m_hero).at(1) == "ADD:456");
    f.tick();
    const auto targetMessages = messages(f.m_world,f.m_monster);
    CHECK(std::find(targetMessages.begin(),targetMessages.end(),"ADD:789") != targetMessages.end());
}

// 配置查表使用英雄物攻标准，召唤链只改变标准伤害来源。
TEST_CASE("Damage real config standard hurt and summon ownership")
{
    HitFixture f; auto& ecs = f.m_world.ecs();
    const auto& row = conf::Config::instance().get<conf::SkillHurtConfig>(1);
    CHECK(hit::standardHurt(f.m_world,f.m_hero) == "800"_fx + Fixed::fromInt(row.hurt));
    ecs.get<Attributes>(f.m_monster).basic[15] = "17"_fx;
    CHECK(hit::standardHurt(f.m_world,f.m_monster) == "17"_fx + Fixed::fromInt(row.monsterHurt));
    ecs.get<RoleIdentity>(f.m_monster).roleType = 5;
    ecs.get<RoleIdentity>(f.m_monster).m_owner = f.m_hero;
    CHECK(hit::standardHurt(f.m_world,f.m_monster) == hit::standardHurt(f.m_world,f.m_hero));
    ecs.get<RoleIdentity>(f.m_monster).m_owner = {};
    CHECK(hit::standardHurt(f.m_world,f.m_monster) == Fixed{});
}
