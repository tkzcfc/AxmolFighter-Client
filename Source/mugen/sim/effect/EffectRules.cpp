#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 由特效配置与主人变换计算放置位姿（移植旧实现 setCustomPosition）。
void placement(const conf::battle::EffectDef& row, const Transform& source, const MapRegion& region, Transform& target)
{
    target.m_towardX = row.orientationX == 0 ? -1 : row.orientationX == 1 ? 1 : source.m_towardX;
    target.m_vectorX = row.orientationX == 0 ? "-1"_fx : row.orientationX == 1 ? "1"_fx : source.m_vectorX;
    target.m_vectorZ = row.orientationZ == 1 ? "1"_fx : source.m_vectorZ;
    const auto axis   = [&](size_t i) { return i < row.relativePosition.size() ? row.relativePosition[i] : Fixed{}; };
    FixedVec3 position{axis(0), axis(1), axis(2)};
    const auto& ranges = region.m_moveRanges;
    if (row.positionType == 0 && !ranges.empty())
    {
        position.x = ranges.front().m_x + ranges.front().m_width * position.x;
        position.z = ranges.front().m_y + ranges.front().m_height * position.z;
    }
    if (row.positionType == 0 || row.positionType == 1)
        position.x += Fixed::fromInt(target.m_towardX);
    else
    {
        position.x = source.m_position.x + Fixed::fromInt(target.m_towardX) * position.x;
        position.y += source.m_position.y;
        position.z += source.m_position.z - "1"_fx;
    }
    target.m_position = position;
}
}  // namespace

namespace effect::rules
{
// 由主人动作生成特效。
void spawnFromRole(World& world, Entity owner, int32_t effectId, const SourceContext& source)
{
    const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
    if (!row)
        return;
    SpawnRequest request;
    request.m_kind     = SpawnKind::Effect;
    request.m_configId = effectId;
    request.m_owner    = owner;
    request.m_source   = source;
    Transform target;
    placement(*row, world.ecs().get<Transform>(owner), world.ecs().singleton<MapRegion>(), target);
    request.m_position = target.m_position;
    request.m_towardX  = target.m_towardX;
    request.m_vectorX  = target.m_vectorX;
    request.m_vectorZ  = target.m_vectorZ;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}

// 后继特效以父特效为放置基准，归属仍是父特效的主人。
void spawnChild(World& world, Entity parent, int32_t effectId)
{
    const auto& info = world.ecs().get<EffectInfo>(parent);
    const auto* row  = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
    if (!row)
        return;
    SpawnRequest request;
    request.m_kind     = SpawnKind::Effect;
    request.m_configId = effectId;
    request.m_owner    = info.m_owner;
    request.m_source.m_skillId     = info.m_skillId;
    request.m_source.m_baseSkillId = info.m_baseSkillId;
    request.m_source.m_slot        = info.m_slot;
    Transform target;
    placement(*row, world.ecs().get<Transform>(parent), world.ecs().singleton<MapRegion>(), target);
    request.m_position = target.m_position;
    request.m_towardX  = target.m_towardX;
    request.m_vectorX  = target.m_vectorX;
    request.m_vectorZ  = target.m_vectorZ;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}

// 生成受击特效。同目标已有实例时改放到新接触点并重播动作，否则新建。
void spawnHitEffect(World& world, Entity owner, Entity sourceEffect, int32_t effectId, Entity hitEntity,
                    const SourceContext& source, const ContactEvent& contact)
{
    const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
    if (!row)
        return;
    const auto& targetPos = world.ecs().get<Transform>(hitEntity).m_position;
    const auto& facing    = world.ecs().get<Transform>(sourceEffect);
    const FixedVec3 position{contact.m_point.x, contact.m_point.z - targetPos.z, targetPos.z - Fixed::fromInt(1)};
    Entity reused;
    world.ecs().each<EffectInfo>([&](Entity entity, const EffectInfo& info) {
        if (reused.value == 0 && info.m_configId == effectId && info.m_hitEntity == hitEntity &&
            !world.ecs().has<Despawning>(entity))
            reused = entity;
    });
    if (reused.value != 0)
    {
        auto& transform        = world.ecs().get<Transform>(reused);
        transform.m_position   = position;
        transform.m_towardX    = facing.m_towardX;
        transform.m_vectorX    = facing.m_vectorX;
        transform.m_vectorZ    = facing.m_vectorZ;
        auto& info             = world.ecs().get<EffectInfo>(reused);
        info.m_skillId         = source.m_skillId;
        info.m_baseSkillId     = source.m_baseSkillId;
        info.m_slot            = source.m_slot;
        const auto& timeline   = world.ecs().get<ActionTimeline>(reused);
        if (!timeline.m_clips.empty())
            timeline::rules::enterClip(world, reused, timeline.m_clips.front(), true, true);
        return;
    }
    SpawnRequest request;
    request.m_kind             = SpawnKind::Effect;
    request.m_configId         = effectId;
    request.m_owner            = owner;
    request.m_source           = source;
    request.m_position         = position;
    request.m_towardX          = facing.m_towardX;
    request.m_vectorX          = facing.m_vectorX;
    request.m_vectorZ          = facing.m_vectorZ;
    request.m_hitEntity        = hitEntity;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}
}  // namespace effect::rules
}  // namespace mugen::sim
