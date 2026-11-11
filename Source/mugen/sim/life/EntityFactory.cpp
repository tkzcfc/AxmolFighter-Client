#include "mugen/sim/life/EntityFactory.h"
#include "mugen/sim/World.h"
#include "mugen/sim/role/RoleSpawn.h"
#include "mugen/sim/skill/SkillRules.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/avatar/AvatarPaths.h"

namespace mugen::sim
{
namespace
{
// 由 resSpineId 解析动作文件。
std::string motionFile(int32_t resSpineId)
{
    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    return spine ? AvatarPaths::motionFileFromSpine(spine->fileName) : std::string{};
}
// 骨骼缩放。碰撞框按 Transform.m_scale 换到世界坐标。
Fixed spineScale(int32_t resSpineId)
{
    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(resSpineId);
    return spine ? spine->scale : Fixed::fromInt(1);
}

// 由 SpawnRequest 装配一个角色实体（原型装配，移植旧实现 RoleSpawner::spawnRole）。
Entity materializeRole(World& world, const SpawnRequest& request, SpawnStage stage)
{
    const auto* row = conf::Config::instance().find<conf::battle::RoleDef>(request.m_configId);
    if (!row)
    {
        MG_LOG_E("SpawnFlush: missing role config {}", request.m_configId);
        MG_ASSERT(row != nullptr && "SpawnFlush: missing role config");
        return {};
    }
    auto& ecs = world.ecs();
    const Entity entity = ecs.create();

    Identity identity;
    identity.m_kind        = SpawnKind::Role;
    identity.m_configId    = request.m_configId;
    identity.m_camp        = request.m_camp != 0 ? request.m_camp : row->monsterCamps;
    identity.m_hostileMask = role::rules::hostileMask(identity.m_camp);
    identity.m_roleType    = row->roleType;
    identity.m_level       = request.m_level;
    if (request.m_hasPlayerSlot)
        identity.m_playerSlot = request.m_playerSlot;
    identity.m_owner   = request.m_owner;
    identity.m_born    = {world.frame(), stage};
    identity.m_ordinal = ecs.singleton<SpawnRegistry>().next(request.m_configId);
    ecs.add(entity, identity);

    Transform transform;
    transform.m_position = request.m_position;
    transform.m_towardX  = request.m_towardX == -1 ? -1 : 1;
    transform.m_vectorX  = Fixed::fromInt(transform.m_towardX);
    transform.m_vectorZ  = request.m_vectorZ;
    transform.m_scale    = spineScale(row->resSpineId);
    ecs.add(entity, transform);

    MotionState motion;
    motion.m_motionFile = motionFile(row->resSpineId);
    motion.m_motionName = "stand";
    motion.m_timeMs     = {};
    motion.m_loop       = true;
    motion.m_playing    = true;
    motion.m_rate       = Fixed::fromInt(1);
    ecs.add(entity, motion);

    Stats stats;
    stats.m_base = role::rules::loadAttributes(*row, request.m_level);
    ecs.add(entity, stats);

    Health health;
    health.m_maxHp  = stats.maxHp();
    health.m_hp     = health.m_maxHp;
    health.m_prevHp = health.m_hp;
    health.m_prevEp = health.m_ep;
    ecs.add(entity, health);

    ecs.add(entity, StatusFlags{});
    ecs.add(entity, Pause{});
    ecs.add(entity, Intent{});
    ecs.add(entity, BuffList{});
    ecs.add(entity, AbilityList{});
    ecs.add(entity, TauntTable{});
    ecs.add(entity, ComboMeter{});
    ecs.add(entity, RageState{});

    RoleState roleState;
    roleState.m_state = RoleStateId::Idle;
    roleState.m_prev  = RoleStateId::Idle;
    ecs.add(entity, roleState);

    HitState hitState;
    hitState.m_rigidity  = Fixed::fromInt(row->rigidity);
    hitState.m_weight    = row->weight;
    hitState.m_fatigue   = Fixed::fromInt(row->fatigue);
    hitState.m_hitCounts = row->hitCount;
    hitState.m_stiffTime = Fixed::fromInt(row->hitStiffTime);
    ecs.add(entity, hitState);

    ecs.add(entity, Displacement{});

    Hitboxes hitboxes;
    hitboxes.m_radius = Fixed::fromInt(row->radius);
    ecs.add(entity, hitboxes);

    if (identity.m_playerSlot)
    {
        PlayerControl control;
        control.m_slot = *identity.m_playerSlot;
        ecs.add(entity, control);
        ecs.singleton<PlayerSlots>().m_entities[*identity.m_playerSlot] = entity;
    }
    // 玩家装配离线技能簿（职业 → skill_tree/skill_leaf）；怪物无技能簿，仅挂施法状态。
    const CharacterClass occupation = type_conversions::toCharacterClass(request.m_configId);
    if (occupation != CharacterClass::kUnknown)
        skill::rules::buildSkillBook(world, entity, occupation);
    else
    {
        ecs.add(entity, SkillCast{});
        ecs.add(entity, ActionTimeline{});
    }
    return entity;
}

// 由 SpawnRequest 装配一个特效实体（移植旧实现 EffectActor::create）。
Entity materializeEffect(World& world, const SpawnRequest& request, SpawnStage stage)
{
    const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(request.m_configId);
    if (!row)
    {
        MG_LOG_E("SpawnFlush: missing effect config {}", request.m_configId);
        MG_ASSERT(row != nullptr && "SpawnFlush: missing effect config");
        return {};
    }
    auto& ecs = world.ecs();
    const Entity entity = ecs.create();

    Identity identity;
    identity.m_kind        = SpawnKind::Effect;
    identity.m_configId    = request.m_configId;
    // 与 sim_legacy EffectActor::create 一致：特效身份固定 camp=1、hostileMask=30，不继承主人。
    identity.m_camp        = 1;
    identity.m_hostileMask = 30;
    identity.m_roleType    = 0;
    identity.m_owner       = request.m_owner;
    identity.m_born        = {world.frame(), stage};
    identity.m_ordinal     = ecs.singleton<SpawnRegistry>().next(request.m_configId);
    ecs.add(entity, identity);

    Transform transform;
    transform.m_position = request.m_position;
    transform.m_towardX  = request.m_towardX == -1 ? -1 : 1;
    transform.m_vectorX  = request.m_vectorX;
    transform.m_vectorZ  = request.m_vectorZ;
    transform.m_scale    = spineScale(row->resSpineId);
    ecs.add(entity, transform);

    MotionState motion;
    motion.m_motionFile = motionFile(row->resSpineId);
    ecs.add(entity, motion);

    EffectInfo info;
    info.m_configId    = request.m_configId;
    info.m_owner       = request.m_owner;
    info.m_skillId     = request.m_source.m_skillId;
    info.m_baseSkillId = request.m_source.m_baseSkillId;
    info.m_slot            = request.m_source.m_slot;
    info.m_attributeSource = request.m_owner;
    // 没有后继的受击特效记住目标，后续命中复用同一实例。
    if (row->nextEffectId == -1)
        info.m_hitEntity = request.m_hitEntity;
    ecs.add(entity, info);
    ecs.add(entity, Pause{});

    Hitboxes hitboxes;
    hitboxes.m_radius = Fixed::fromInt(row->radius);
    ecs.add(entity, hitboxes);

    ecs.add(entity, Displacement{});
    ecs.add(entity, HitTracker{});

    Lifetime lifetime;
    lifetime.m_autoRelease = row->autoRelease;
    ecs.add(entity, lifetime);

    if (row->follow != 0)
    {
        Follow follow;
        follow.m_target = request.m_owner;
        follow.m_mode   = row->follow;
        ecs.add(entity, follow);
    }

    ActionTimeline timeline;
    timeline.m_clips    = row->clips;
    timeline.m_index    = 0;
    timeline.m_finished = row->clips.empty();
    const int32_t firstClip = timeline.m_finished ? 0 : timeline.m_clips.front();
    ecs.add(entity, std::move(timeline));
    // 出生时同步进入首片段；下一次特效遍只消费延迟标记，不在进入帧推进。
    if (firstClip != 0 && !timeline::rules::enterClip(world, entity, firstClip, true, true))
        ecs.get<ActionTimeline>(entity).m_finished = true;

    ecs.add(entity, ContactMemory{});
    return entity;
}
}  // namespace

namespace life::rules
{
// 排空 SpawnRequest 队列并按出生阶段实体化。
void spawnFlush(World& world, SpawnStage stage)
{
    auto& queue = world.ecs().singleton<EventQueue<SpawnRequest>>();
    for (const SpawnRequest& request : queue.m_items)
    {
        Entity entity;
        if (request.m_kind == SpawnKind::Role)
            entity = materializeRole(world, request, stage);
        else if (request.m_kind == SpawnKind::Effect)
            entity = materializeEffect(world, request, stage);
        if (entity.value != 0)
        {
            SpawnEvent event;
            event.m_entity   = entity;
            event.m_kind     = request.m_kind;
            event.m_configId = request.m_configId;
            event.m_owner    = request.m_owner;
            event.m_ordinal  = world.ecs().get<Identity>(entity).m_ordinal;
            world.ecs().singleton<EventQueue<SpawnEvent>>().push(event);
        }
    }
    queue.clear();
}
}  // namespace life::rules
}  // namespace mugen::sim
