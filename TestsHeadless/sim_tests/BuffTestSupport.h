#pragma once

#include "doctest.h"
#include "../support/ContentVfs.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/combat/HealthRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/battle/AbilityDef.h"

#include <algorithm>
#include <vector>

namespace buff_test
{
using namespace mugen;
using namespace mugen::sim;
using namespace mugen::conf::battle;

// 测试用 Buff / 能力 id，避开源表。
inline int32_t nextBuffId()
{
    static int32_t n = 99010000;
    return n++;
}

inline int32_t nextAbilityId()
{
    static int32_t n = 99020000;
    return n++;
}

// 挂载 VFS 并加载配置（幂等）。
inline void loadAssets()
{
    REQUIRE(test::ensureContentVfs());
    auto& config = conf::Config::instance();
    if (!config.loaded())
        REQUIRE(config.load("/mugen/config/config.bin"));
}

inline std::vector<BuffDef>& buffTable()
{
    return const_cast<std::vector<BuffDef>&>(conf::Config::instance().table<BuffDef>());
}

inline std::vector<AbilityDef>& abilityTable()
{
    return const_cast<std::vector<AbilityDef>&>(conf::Config::instance().table<AbilityDef>());
}

// 把自定义行插入已排序的配置表，析构时撤掉，避免污染其它用例。
struct BuffDefGuard
{
    std::vector<int32_t> m_ids;
    ~BuffDefGuard() { clear(); }
    BuffDefGuard() = default;
    BuffDefGuard(const BuffDefGuard&) = delete;
    BuffDefGuard& operator=(const BuffDefGuard&) = delete;

    int32_t add(BuffDef def)
    {
        const int32_t id = def.m_id;
        auto& rows       = buffTable();
        const auto it    = std::lower_bound(rows.begin(), rows.end(), id,
                                            [](const BuffDef& row, int32_t key) { return row.m_id < key; });
        if (it != rows.end() && it->m_id == id)
            *it = std::move(def);
        else
            rows.insert(it, std::move(def));
        m_ids.push_back(id);
        return id;
    }

    void clear()
    {
        auto& rows = buffTable();
        for (const int32_t id : m_ids)
            std::erase_if(rows, [id](const BuffDef& row) { return row.m_id == id; });
        m_ids.clear();
    }
};

struct AbilityDefGuard
{
    std::vector<int32_t> m_ids;
    ~AbilityDefGuard() { clear(); }
    AbilityDefGuard() = default;
    AbilityDefGuard(const AbilityDefGuard&) = delete;
    AbilityDefGuard& operator=(const AbilityDefGuard&) = delete;

    int32_t add(AbilityDef def)
    {
        const int32_t id = def.m_id;
        auto& rows       = abilityTable();
        const auto it    = std::lower_bound(rows.begin(), rows.end(), id,
                                            [](const AbilityDef& row, int32_t key) { return row.m_id < key; });
        if (it != rows.end() && it->m_id == id)
            *it = std::move(def);
        else
            rows.insert(it, std::move(def));
        m_ids.push_back(id);
        return id;
    }

    void clear()
    {
        auto& rows = abilityTable();
        for (const int32_t id : m_ids)
            std::erase_if(rows, [id](const AbilityDef& row) { return row.m_id == id; });
        m_ids.clear();
    }
};

// 立即生效、Enter 合闩 / Exit 撤销的空 Buff。
inline BuffDef makeBuff(int32_t id)
{
    BuffDef def;
    def.m_id                             = id;
    def.m_status                         = LowerStatus::Lowered;
    def.m_admission.m_ruleId             = id;
    def.m_admission.m_subType            = 0;
    def.m_admission.m_maxStacks          = 99;
    def.m_admission.m_probability        = Fixed::fromInt(100);
    def.m_lifecycle.m_executeType        = 1;
    def.m_activation.m_latchOn           = static_cast<int32_t>(BuffHook::Enter);
    def.m_activation.m_releaseOn         = static_cast<int32_t>(BuffHook::Exit);
    return def;
}

inline ValueExpr constVal(Fixed a)
{
    ValueExpr expr;
    expr.m_kind = ValueKind::Const;
    expr.m_a    = a;
    return expr;
}

inline BuffOp flagOp(BuffFlag flag)
{
    BuffOp op;
    op.m_kind = BuffOpKind::Flag;
    op.m_flag = flag;
    return op;
}

inline BuffOp statOp(BuffStat stat, ValueExpr expr, bool live = false)
{
    BuffOp op;
    op.m_kind  = BuffOpKind::Stat;
    op.m_stat  = stat;
    op.m_value = expr;
    op.m_live  = live;
    return op;
}

inline BuffOp resourceOp(ResourceId res, ValueExpr expr)
{
    BuffOp op;
    op.m_kind  = BuffOpKind::Resource;
    op.m_res   = res;
    op.m_value = expr;
    return op;
}

// 最小可准入角色：Buff 解释器读到的组件都给上。
inline Entity spawnActor(World& world, int32_t roleType, int32_t camp, int32_t hostileMask, int32_t level = 100)
{
    const Entity entity = world.ecs().create();
    IdentityComponent identity;
    identity.m_kind        = SpawnKind::Role;
    identity.m_roleType    = roleType;
    identity.m_camp        = camp;
    identity.m_hostileMask = hostileMask;
    identity.m_level       = level;
    world.ecs().add(entity, identity);

    StatsComponent stats;
    stats.m_base[4]  = Fixed::fromInt(10000);
    stats.m_base[5]  = Fixed::fromInt(200);
    stats.m_base[15] = Fixed::fromInt(50);
    world.ecs().add(entity, stats);

    HealthComponent health;
    health.m_maxHp  = stats.maxHp();
    health.m_hp     = health.m_maxHp;
    health.m_prevHp = health.m_hp;
    health.m_maxMp  = Fixed::fromInt(100);
    health.m_mp     = health.m_maxMp;
    health.m_maxEp  = Fixed::fromInt(1000);
    health.m_ep     = health.m_maxEp;
    health.m_prevEp = health.m_ep;
    world.ecs().add(entity, health);

    world.ecs().add(entity, StatusFlagsComponent{});
    world.ecs().add(entity, BuffListComponent{});
    world.ecs().add(entity, AbilityListComponent{});
    world.ecs().add(entity, TauntTableComponent{});
    world.ecs().add(entity, ComboMeterComponent{});
    world.ecs().add(entity, RageStateComponent{});
    world.ecs().add(entity, SkillBookComponent{});
    world.ecs().add(entity, SkillCastComponent{});
    world.ecs().add(entity, ActionTimelineComponent{});
    world.ecs().add(entity, RoleStateComponent{});
    world.ecs().add(entity, TransformComponent{});
    world.ecs().add(entity, DisplacementComponent{});
    world.ecs().add(entity, PauseComponent{});
    world.ecs().add(entity, MotionStateComponent{});
    return entity;
}

inline BuffInstance* findBuff(World& world, Entity entity, int32_t id)
{
    if (!world.ecs().has<BuffListComponent>(entity))
        return nullptr;
    for (auto& inst : world.ecs().get<BuffListComponent>(entity).m_instances)
        if (inst.m_id == id && !inst.m_removing)
            return &inst;
    return nullptr;
}

inline int32_t stackOf(World& world, Entity entity, int32_t id)
{
    const auto* inst = findBuff(world, entity, id);
    return inst ? inst->m_stacks : 0;
}

inline Fixed statSum(World& world, Entity entity, int32_t stat)
{
    return world.ecs().get<StatsComponent>(entity).sumMods(stat);
}

inline bool hasFlag(World& world, Entity entity, ExtraState state)
{
    return world.ecs().get<StatusFlagsComponent>(entity).active(state);
}

inline int32_t eventCount(World& world, BuffPhase phase, int32_t buffId = 0)
{
    int32_t count = 0;
    for (const auto& event : world.ecs().singleton<EventQueue<BuffEvent>>().m_items)
        if (event.m_phase == phase && (buffId == 0 || event.m_buffId == buffId))
            ++count;
    return count;
}

struct Harness
{
    DrawPolicy policy;
    World world;
    Entity self;
    Entity other;
    BuffDefGuard buffs;
    AbilityDefGuard abilities;

    Harness()
    {
        loadAssets();
        policy.m_probability = Fixed::fromInt(0);
        WorldSetup setup;
        setup.seed        = 31;
        setup.mode        = WorldMode::Battle;
        setup.m_aiEnabled = false;
        world.init(setup);
        world.setDrawPolicy(&policy);
        self  = spawnActor(world, 1, 2, 4);
        other = spawnActor(world, 4, 4, 2);
    }

    int32_t add(BuffDef def) { return buffs.add(std::move(def)); }

    int32_t addAbility(AbilityDef def) { return abilities.add(std::move(def)); }

    void admit(int32_t id, const BuffContext& ctx = {}) { buff::rules::admit(world, self, id, ctx); }

    void admitTo(Entity entity, int32_t id, const BuffContext& ctx = {})
    {
        buff::rules::admit(world, entity, id, ctx);
    }

    void remove(int32_t id) { buff::rules::remove(world, self, id); }

    void fire(TriggerPoint point, const BuffContext& ctx = {}) { buff::rules::fire(world, self, point, ctx); }

    void tickBuff(int32_t n = 1)
    {
        for (int32_t i = 0; i < n; ++i)
            buff::rules::tick(world);
    }

    void tickWorld(int32_t n = 1)
    {
        for (int32_t i = 0; i < n; ++i)
        {
            FrameInput input;
            input.frame = world.frame() + 1;
            world.tick(input);
        }
    }

    // 把 Buff 计时的一帧改成整秒，便于测到期。
    void setTickSeconds(int32_t seconds)
    {
        auto& clock   = world.ecs().singleton<WorldClock>();
        clock.m_dtMs  = Fixed::fromInt(seconds) * Fixed::fromInt(1000);
        clock.m_dtSec = Fixed::fromInt(seconds);
    }
};
}  // namespace buff_test
