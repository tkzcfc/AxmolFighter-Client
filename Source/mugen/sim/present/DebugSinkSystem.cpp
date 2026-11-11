// [11.2] DebugSink
// 读：全部事实队列、`Transform`（位移终点）、`WorldClock`（顿帧毫秒换帧）、`SkillBook`（槽位下标换字母）
// 写：追踪行（本系统拥有生命周期，帧初清空）
// 消费：—
// 产生：—
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"

namespace mugen::sim
{
namespace
{
// 实体语义键：role:<configId>#<ordinal> 或 fx:<configId>#<ordinal>。
std::string entityKey(World& world, Entity entity)
{
    if (entity.value == 0)
        return "-";
    const auto& identity = world.ecs().get<Identity>(entity);
    const char* stem     = identity.m_kind == SpawnKind::Effect ? "fx" : "role";
    return std::string(stem) + ':' + std::to_string(identity.m_configId) + '#' +
           std::to_string(identity.m_ordinal);
}
// 定点转毫单位整数。
int64_t toMilli(Fixed value)
{
    const int64_t scaled = value.raw * 1000;
    int64_t result       = scaled / Fixed::kOne;
    if (scaled % Fixed::kOne != 0 && scaled < 0)
        --result;
    return result;
}
// 命中结果文本。
const char* hitResultName(HitResult result)
{
    switch (result)
    {
    case HitResult::Hit:
        return "hit";
    case HitResult::Evade:
        return "evade";
    case HitResult::Miss:
        return "miss";
    case HitResult::NoDamage:
        return "none";
    }
    return "hit";
}
// 槽位下标 → 槽位字母。
std::string slotLetter(World& world, const CastEvent& event)
{
    if (!world.ecs().has<SkillBook>(event.m_entity))
        return "?";
    const auto& slots = world.ecs().get<SkillBook>(event.m_entity).m_slots;
    if (event.m_slot < 0 || static_cast<size_t>(event.m_slot) >= slots.size())
        return "?";
    return slots[static_cast<size_t>(event.m_slot)];
}
// 顿帧帧数 = ceil(freezeMs / dtMs)。
int32_t stopFrames(Fixed freezeMs, Fixed dtMs)
{
    return static_cast<int32_t>((freezeMs.raw + dtMs.raw - 1) / dtMs.raw);
}
}  // namespace

// 系统 DebugSink：11.2 行，见 DESIGN.md §3.2。把事实队列翻译成追踪行。
void runDebugSink(World& world)
{
    world.clearTraceLines();
    auto& ecs        = world.ecs();
    const Fixed dtMs = ecs.singleton<WorldClock>().m_dtMs;
    for (const CastEvent& event : ecs.singleton<EventQueue<CastEvent>>().m_items)
    {
        const std::string key = entityKey(world, event.m_entity);
        if (event.m_phase == CastPhase::Began)
            world.appendTrace("cast " + key + ' ' + std::to_string(event.m_skillId) + ' ' + slotLetter(world, event));
        else if (event.m_phase == CastPhase::Ended)
            world.appendTrace("castend " + key + ' ' + std::to_string(event.m_skillId));
        else if (event.m_phase == CastPhase::Cancelled)
            world.appendTrace("castend " + key + ' ' + std::to_string(event.m_skillId));
    }
    for (const SpawnEvent& event : ecs.singleton<EventQueue<SpawnEvent>>().m_items)
    {
        const std::string owner = entityKey(world, event.m_owner);
        const std::string key   = entityKey(world, event.m_entity);
        const auto& identity    = ecs.get<Identity>(event.m_entity);
        if (event.m_kind == SpawnKind::Effect || event.m_kind == SpawnKind::Role)
            if (identity.m_born.m_frame != 0)
                world.appendTrace("spawn " + owner + ' ' + key);
    }
    for (const HitEvent& event : ecs.singleton<EventQueue<HitEvent>>().m_items)
    {
        const std::string effect   = entityKey(world, event.m_effect);
        const std::string target   = entityKey(world, event.m_target);
        const std::string attacker = entityKey(world, event.m_attacker);
        world.appendTrace("hit " + effect + ' ' + target + ' ' + std::to_string(event.m_hitDefId) + ' ' +
                          hitResultName(event.m_result));
        if (event.m_result == HitResult::Hit)
            world.appendTrace("dmg " + attacker + ' ' + target + ' ' + std::to_string(event.m_effectId) + ' ' +
                              std::to_string(event.m_damage.toInt()) + ' ' + (event.m_critical ? '1' : '0'));
    }
    for (const HpEvent& event : ecs.singleton<EventQueue<HpEvent>>().m_items)
    {
        if (event.m_cause == HpCause::Hit)
            continue;
        const std::string key = entityKey(world, event.m_entity);
        const char* reason = "other";
        if (event.m_cause == HpCause::Dot)
            reason = "dot";
        else if (event.m_cause == HpCause::Heal)
            reason = "heal";
        else if (event.m_cause == HpCause::Shield)
            reason = "shield";
        world.appendTrace("hp " + key + ' ' + std::to_string(event.m_delta.toInt()) + ' ' + reason);
    }
    for (const HitStopEvent& event : ecs.singleton<EventQueue<HitStopEvent>>().m_items)
        world.appendTrace("stop " + entityKey(world, event.m_entity) + ' ' +
                          std::to_string(stopFrames(event.m_freezeMs, dtMs)));
    for (const BuffEvent& event : ecs.singleton<EventQueue<BuffEvent>>().m_items)
    {
        const std::string key = entityKey(world, event.m_entity);
        if (event.m_phase == BuffPhase::Added)
            world.appendTrace("buff + " + key + ' ' + std::to_string(event.m_buffId));
        else if (event.m_phase == BuffPhase::Removed)
            world.appendTrace("buff - " + key + ' ' + std::to_string(event.m_buffId));
    }
    for (const DisplacementEvent& event : ecs.singleton<EventQueue<DisplacementEvent>>().m_items)
    {
        if (!ecs.alive(event.m_entity))
            continue;
        const std::string key = entityKey(world, event.m_entity);
        if (event.m_phase == DispPhase::Began)
            world.appendTrace("disp+ " + key + ' ' + std::to_string(event.m_curveId));
        else
        {
            const auto& pos = ecs.get<Transform>(event.m_entity).m_position;
            world.appendTrace("disp- " + key + ' ' + std::to_string(event.m_curveId) + ' ' +
                              std::to_string(toMilli(pos.x)) + ' ' + std::to_string(toMilli(pos.y)) + ' ' +
                              std::to_string(toMilli(pos.z)));
        }
    }
    for (const DeathEvent& event : ecs.singleton<EventQueue<DeathEvent>>().m_items)
        world.appendTrace("death " + entityKey(world, event.m_entity));
}
}  // namespace mugen::sim
