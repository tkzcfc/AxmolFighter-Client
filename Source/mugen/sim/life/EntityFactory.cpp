#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/World.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/room/RoomRules.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/conf/GameDef.h"
#include "mugen/avatar/AvatarPaths.h"
#include <algorithm>

namespace mugen::sim
{
namespace life::rules
{
void inheritBuffs(World& world, Entity from, Entity to, int32_t flag);
}
namespace
{
// 由 resSpineId 解析动作文件。
std::string motionFile(int32_t resSpineId)
{
    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    return spine ? AvatarPaths::motionFileFromSpine(spine->fileName) : std::string{};
}
// 骨骼缩放。碰撞框按 TransformComponent.scale 换到世界坐标。
Fixed spineScale(int32_t resSpineId)
{
    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    return spine ? spine->scale : Fixed::fromInt(1);
}
// 召唤根：主人若也是召唤则取其根，否则主人自己。
Entity summonRoot(World& world, Entity owner)
{
    if (owner.value == 0 || !world.ecs().alive(owner))
        return owner;
    if (const auto* summon = world.ecs().tryGet<SummonComponent>(owner))
        return summon->root.value != 0 ? summon->root : owner;
    if (const auto* identity = world.ecs().tryGet<IdentityComponent>(owner))
        if (identity->rootOwner.value != 0)
            return identity->rootOwner;
    return owner;
}

// 由 SpawnRequest 装配一个角色实体（原型装配，移植旧实现 RoleSpawner::spawnRole）。
Entity materializeRole(World& world, const SpawnRequest& request, SpawnStage stage)
{
    const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(request.configId);
    if (!row)
    {
        MG_LOG_E("SpawnFlushSystem: missing role config {}", request.configId);
        MG_ASSERT(row != nullptr && "SpawnFlushSystem: missing role config");
        return {};
    }
    auto& ecs = world.ecs();
    const Entity entity = ecs.create();

    IdentityComponent identity;
    identity.kind        = SpawnKind::Role;
    identity.configId    = request.configId;
    identity.camp        = request.camp != 0 ? request.camp : row->monsterCamps;
    identity.hostileMask = role::rules::hostileMask(identity.camp);
    identity.roleType    = row->roleType;
    identity.level       = request.level;
    identity.aiDifficulty = request.aiDifficulty;
    if (request.hasPlayerSlot)
        identity.playerSlot = request.playerSlot;
    identity.owner     = request.owner;
    identity.rootOwner = summonRoot(world, request.owner);
    identity.born      = {world.frame(), stage};
    identity.ordinal   = ecs.singleton<SpawnRegistry>().next(request.configId);
    ecs.add(entity, identity);

    TransformComponent transform;
    transform.position = request.position;
    transform.towardX  = request.towardX == -1 ? -1 : 1;
    transform.vectorX  = request.vectorX != Fixed{} ? request.vectorX : Fixed::fromInt(transform.towardX);
    transform.vectorZ  = request.vectorZ;
    transform.scale    = spineScale(row->resSpineId);
    ecs.add(entity, transform);

    MotionStateComponent motion;
    motion.motionFile = motionFile(row->resSpineId);
    motion.motionName = "stand";
    motion.timeMs     = {};
    motion.loop       = true;
    motion.playing    = true;
    motion.rate       = Fixed::fromInt(1);
    ecs.add(entity, motion);

    StatsComponent stats;
    stats.base = role::rules::loadAttributes(*row, request.level);
    if (row->roleType == static_cast<int32_t>(EntityRoleType::kSummon) && request.owner.value != 0 &&
        ecs.has<StatsComponent>(request.owner))
    {
        const auto& ownerStats = ecs.get<StatsComponent>(request.owner);
        for (size_t i = 0; i < stats.base.size(); ++i)
            stats.base[i] = ownerStats.value(static_cast<int32_t>(i)) *
                              (i < row->attributeRate.size() ? row->attributeRate[i] : Fixed::fromInt(1));
    }
    ecs.add(entity, stats);

    HealthComponent health;
    health.maxHp  = stats.maxHp();
    health.hp     = health.maxHp;
    health.prevHp = health.hp;
    health.prevEp = health.ep;
    ecs.add(entity, health);

    ecs.add(entity, StatusFlagsComponent{});
    ecs.add(entity, PauseComponent{});
    ecs.add(entity, IntentComponent{});
    ecs.add(entity, BuffListComponent{});
    ecs.add(entity, AbilityListComponent{});
    ecs.add(entity, TauntTableComponent{});
    ecs.add(entity, ComboMeterComponent{});
    ecs.add(entity, RageStateComponent{});

    RoleStateComponent roleState;
    roleState.state = RoleStateId::Idle;
    roleState.prev  = RoleStateId::Idle;
    ecs.add(entity, roleState);

    HitStateComponent hitState;
    hitState.rigidity  = Fixed::fromInt(row->rigidity);
    hitState.weight    = row->weight;
    hitState.fatigue   = Fixed::fromInt(row->fatigue);
    hitState.hitCounts = 1;
    hitState.stiffTime = Fixed::fromInt(row->hitStiffTime);
    ecs.add(entity, hitState);

    ecs.add(entity, DisplacementComponent{});

    HitboxesComponent hitboxes;
    hitboxes.radius = Fixed::fromInt(row->radius);
    hitboxes.depth  = Fixed::fromInt(row->depth);
    ecs.add(entity, hitboxes);

    if (identity.playerSlot)
    {
        PlayerControlComponent control;
        control.slot = *identity.playerSlot;
        ecs.add(entity, control);
        ecs.singleton<PlayerSlots>().entities[*identity.playerSlot] = entity;
    }
    // 玩家装配离线技能簿（职业 → skill_tree/skill_leaf）；怪物无技能簿，仅挂施法状态。
    const CharacterClass occupation = type_conversions::toCharacterClass(request.configId);
    if (occupation != CharacterClass::kUnknown)
        skill::rules::buildSkillBook(world, entity, occupation);
    else
    {
        ecs.add(entity, SkillCastComponent{});
        ecs.add(entity, ActionTimelineComponent{});
    }
    if (request.lifetimeMs > 0 && request.owner.value != 0)
    {
        SummonComponent summon;
        summon.owner        = request.owner;
        summon.root         = identity.rootOwner.value != 0 ? identity.rootOwner : request.owner;
        summon.remainingMs  = Fixed::fromInt(request.lifetimeMs);
        ecs.add(entity, summon);
        life::rules::inheritBuffs(world, request.owner, entity, 1);
    }
    return entity;
}

// 由 SpawnRequest 装配一个特效实体（移植旧实现 EffectActor::create）。
Entity materializeEffect(World& world, const SpawnRequest& request, SpawnStage stage)
{
    const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(request.configId);
    if (!row)
    {
        MG_LOG_E("SpawnFlushSystem: missing effect config {}", request.configId);
        MG_ASSERT(row != nullptr && "SpawnFlushSystem: missing effect config");
        return {};
    }
    auto& ecs = world.ecs();
    const Entity entity = ecs.create();

    IdentityComponent identity;
    identity.kind        = SpawnKind::Effect;
    identity.configId    = request.configId;
    // 特效自身身份固定为中立阵营（camp=1、hostileMask=30），敌对过滤在碰撞阶段解析至最终主人。
    identity.camp        = 1;
    identity.hostileMask = 30;
    identity.roleType    = 0;
    identity.owner       = request.owner;
    identity.born        = {world.frame(), stage};
    identity.ordinal     = ecs.singleton<SpawnRegistry>().next(request.configId);
    ecs.add(entity, identity);

    TransformComponent transform;
    transform.position = request.position;
    transform.towardX  = request.towardX == -1 ? -1 : 1;
    transform.vectorX  = request.vectorX;
    transform.vectorZ  = request.vectorZ;
    transform.scale    = spineScale(row->resSpineId);
    ecs.add(entity, transform);

    MotionStateComponent motion;
    motion.motionFile = motionFile(row->resSpineId);
    ecs.add(entity, motion);

    EffectInfoComponent info;
    info.configId    = request.configId;
    info.owner       = request.owner;
    info.skillId     = request.source.skillId;
    info.baseSkillId = request.source.baseSkillId;
    info.slot            = request.source.slot;
    info.attributeSource = request.owner;
    // 没有后继的受击特效记住目标，后续命中复用同一实例。
    if (row->nextEffectId == -1)
        info.hitEntity = request.hitEntity;
    ecs.add(entity, info);
    ecs.add(entity, PauseComponent{});

    HitboxesComponent hitboxes;
    hitboxes.radius = Fixed::fromInt(row->radius);
    hitboxes.depth  = Fixed::fromInt(row->depth);
    ecs.add(entity, hitboxes);

    ecs.add(entity, DisplacementComponent{});
    ecs.add(entity, HitTrackerComponent{});

    LifetimeComponent lifetime;
    lifetime.autoRelease = row->autoRelease;
    ecs.add(entity, lifetime);

    if (row->follow != 0)
    {
        FollowComponent follow;
        follow.target = request.owner;
        follow.mode   = row->follow;
        ecs.add(entity, follow);
    }

    ActionTimelineComponent timeline;
    timeline.clips    = row->clips;
    timeline.index    = 0;
    timeline.finished = row->clips.empty();
    const int32_t firstClip = timeline.finished ? 0 : timeline.clips.front();
    ecs.add(entity, std::move(timeline));
    // 角色时间轴出生后本帧立刻跑特效遍，与旧实现 create 后同帧 update 一致，不 defer。
    // 后继/受击/生命周期在特效遍之后才实体化，第一次推进自然落在下一帧。
    if (firstClip != 0 && !timeline::rules::enterClip(world, entity, firstClip, true, false))
        ecs.get<ActionTimelineComponent>(entity).finished = true;

    ecs.add(entity, ContactMemoryComponent{});
    return entity;
}
}  // namespace

namespace life::rules
{
// 准入 inherit 列表是否含指定标记（1 召唤 / 2 变身）。
bool inheritHas(const conf::battle::BuffDef& def, int32_t flag)
{
    return std::find(def.admission.inherit.begin(), def.admission.inherit.end(), flag) !=
           def.admission.inherit.end();
}

// 把来源身上 inherit 含 flag 的 Buff 同步准入到目标。
void inheritBuffs(World& world, Entity from, Entity to, int32_t flag)
{
    if (!world.ecs().has<BuffListComponent>(from) || !world.ecs().has<BuffListComponent>(to))
        return;
    std::vector<int32_t> ids;
    for (const auto& inst : world.ecs().get<BuffListComponent>(from).instances)
    {
        if (inst.removing)
            continue;
        const auto* def = conf::Config::instance().find<conf::battle::BuffDef>(inst.id);
        if (def && inheritHas(*def, flag))
            ids.push_back(inst.id);
    }
    BuffContext ctx;
    ctx.applicator = from;
    ctx.target     = to;
    for (const int32_t id : ids)
        buff::rules::admit(world, to, id, ctx);
}

// 按请求装配一个实体。
Entity materialize(World& world, const SpawnRequest& request, SpawnStage stage)
{
    if (request.kind == SpawnKind::Role)
        return materializeRole(world, request, stage);
    if (request.kind == SpawnKind::Effect)
        return materializeEffect(world, request, stage);
    if (request.kind == SpawnKind::Obstacle || request.kind == SpawnKind::Goods || request.kind == SpawnKind::Portal)
        return room::rules::materialize(world, request, stage);
    return {};
}

// 排空 SpawnRequest 队列并按出生阶段实体化。
void spawnFlush(World& world, SpawnStage stage)
{
    auto& queue = world.ecs().singleton<EventQueue<SpawnRequest>>();
    for (const SpawnRequest& request : queue.items)
    {
        const Entity entity = materialize(world, request, stage);
        if (entity.value != 0)
        {
            SpawnEvent event;
            event.entity   = entity;
            event.kind     = request.kind;
            event.configId = request.configId;
            event.owner    = request.owner;
            event.ordinal  = world.ecs().get<IdentityComponent>(entity).ordinal;
            world.ecs().singleton<EventQueue<SpawnEvent>>().push(event);
        }
    }
    queue.clear();
}

// 打上回收标签并入队。已标记的实体不重复。
void despawn(World& world, Entity entity, DespawnReason reason)
{
    if (!world.ecs().alive(entity) || world.ecs().has<DespawningComponent>(entity))
        return;
    world.ecs().add(entity, DespawningComponent{});
    world.ecs().singleton<EventQueue<DespawnRequest>>().push({entity, reason});
}
}  // namespace life::rules
}  // namespace mugen::sim
