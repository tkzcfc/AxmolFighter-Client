#include "mugen/sim/effect/EffectRules.h"
#include "mugen/sim/skill/TimelineRules.h"
#include "mugen/conf/ReflectedConfig.h"

namespace mugen::sim
{
namespace
{
// 由特效配置与主人变换计算放置位姿（移植旧实现 setCustomPosition）。
void placement(const conf::battle::EffectDef& row, const TransformComponent& source, const MapRegion& region, TransformComponent& target)
{
    target.towardX = row.orientationX == 0 ? -1 : row.orientationX == 1 ? 1 : source.towardX;
    target.vectorX = row.orientationX == 0 ? "-1"_fx : row.orientationX == 1 ? "1"_fx : source.vectorX;
    target.vectorZ = row.orientationZ == 1 ? "1"_fx : source.vectorZ;
    const auto axis   = [&](size_t i) { return i < row.relativePosition.size() ? row.relativePosition[i] : Fixed{}; };
    FixedVec3 position{axis(0), axis(1), axis(2)};
    const auto& ranges = region.moveRanges;
    if (row.positionType == 0 && !ranges.empty())
    {
        position.x = ranges.front().x + ranges.front().width * position.x;
        position.z = ranges.front().y + ranges.front().height * position.z;
    }
    if (row.positionType == 0 || row.positionType == 1)
        position.x += Fixed::fromInt(target.towardX);
    else
    {
        position.x = source.position.x + Fixed::fromInt(target.towardX) * position.x;
        position.y += source.position.y;
        position.z += source.position.z - "1"_fx;
    }
    target.position = position;
}
}  // namespace

namespace effect::rules
{
// 按特效配置把 entity 放到 base 的相对位置。
void placeFrom(World& world, Entity entity, Entity base, bool ground)
{
    const auto& info = world.ecs().get<EffectInfoComponent>(entity);
    const auto* row  = conf::Config::instance().find<conf::battle::EffectDef>(info.configId);
    if (!row)
        return;
    auto& target = world.ecs().get<TransformComponent>(entity);
    placement(*row, world.ecs().get<TransformComponent>(base), world.ecs().singleton<MapRegion>(), target);
    if (ground)
        target.position.y = {};
}

// 由主人动作生成特效。
void spawnFromRole(World& world, Entity owner, int32_t effectId, const SourceContext& source)
{
    const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
    if (!row)
        return;
    SpawnRequest request;
    request.kind     = SpawnKind::Effect;
    request.configId = effectId;
    request.owner    = owner;
    request.source   = source;
    TransformComponent target;
    placement(*row, world.ecs().get<TransformComponent>(owner), world.ecs().singleton<MapRegion>(), target);
    request.position = target.position;
    request.towardX  = target.towardX;
    request.vectorX  = target.vectorX;
    request.vectorZ  = target.vectorZ;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}

// 后继特效以父特效为放置基准，归属仍是父特效的主人。
void spawnChild(World& world, Entity parent, int32_t effectId)
{
    const auto& info = world.ecs().get<EffectInfoComponent>(parent);
    const auto* row  = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
    if (!row)
        return;
    SpawnRequest request;
    request.kind     = SpawnKind::Effect;
    request.configId = effectId;
    request.owner    = info.owner;
    request.source.skillId     = info.skillId;
    request.source.baseSkillId = info.baseSkillId;
    request.source.slot        = info.slot;
    TransformComponent target;
    placement(*row, world.ecs().get<TransformComponent>(parent), world.ecs().singleton<MapRegion>(), target);
    request.position = target.position;
    request.towardX  = target.towardX;
    request.vectorX  = target.vectorX;
    request.vectorZ  = target.vectorZ;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}

// 生成受击特效。同目标已有实例时改放到新接触点并重播动作，否则新建。
void spawnHitEffect(World& world, Entity owner, Entity sourceEffect, int32_t effectId, Entity hitEntity,
                    const SourceContext& source, const ContactEvent& contact)
{
    const auto* row = conf::Config::instance().find<conf::battle::EffectDef>(effectId);
    if (!row)
        return;
    const auto& facing = world.ecs().get<TransformComponent>(sourceEffect);
    // 接触点是世界坐标：x 横向，y 高度，z 纵深。纵深减 1，让火花画在重叠中心前方。
    const FixedVec3 position{contact.point.x, contact.point.y, contact.point.z - Fixed::fromInt(1)};
    Entity reused;
    world.ecs().each<EffectInfoComponent>([&](Entity entity, const EffectInfoComponent& info) {
        if (reused.value == 0 && info.configId == effectId && info.hitEntity == hitEntity &&
            !world.ecs().has<DespawningComponent>(entity))
            reused = entity;
    });
    if (reused.value != 0)
    {
        auto& transform        = world.ecs().get<TransformComponent>(reused);
        transform.position   = position;
        transform.towardX    = facing.towardX;
        transform.vectorX    = facing.vectorX;
        transform.vectorZ    = facing.vectorZ;
        auto& info             = world.ecs().get<EffectInfoComponent>(reused);
        info.skillId         = source.skillId;
        info.baseSkillId     = source.baseSkillId;
        info.slot            = source.slot;
        const auto& timeline   = world.ecs().get<ActionTimelineComponent>(reused);
        if (!timeline.clips.empty())
            timeline::rules::enterClip(world, reused, timeline.clips.front(), true, true);
        return;
    }
    SpawnRequest request;
    request.kind             = SpawnKind::Effect;
    request.configId         = effectId;
    request.owner            = owner;
    request.source           = source;
    request.position         = position;
    request.towardX          = facing.towardX;
    request.vectorX          = facing.vectorX;
    request.vectorZ          = facing.vectorZ;
    request.hitEntity        = hitEntity;
    world.ecs().singleton<EventQueue<SpawnRequest>>().push(request);
}
}  // namespace effect::rules
}  // namespace mugen::sim
