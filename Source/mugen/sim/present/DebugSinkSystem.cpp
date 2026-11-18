// [11.2] DebugSinkSystem
// 读：全部事实队列、`TransformComponent`（位移终点）、`WorldClock`（顿帧毫秒换帧）、`SkillBookComponent`（槽位下标换字母）
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
    const auto& identity = world.ecs().get<IdentityComponent>(entity);
    const char* stem     = identity.kind == SpawnKind::Effect ? "fx" : "role";
    return std::string(stem) + ':' + std::to_string(identity.configId) + '#' +
           std::to_string(identity.ordinal);
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
    if (!world.ecs().has<SkillBookComponent>(event.entity))
        return "?";
    const auto& slots = world.ecs().get<SkillBookComponent>(event.entity).slots;
    if (event.slot < 0 || static_cast<size_t>(event.slot) >= slots.size())
        return "?";
    return slots[static_cast<size_t>(event.slot)];
}
// 顿帧帧数 = ceil(freezeMs / dtMs)。
int32_t stopFrames(Fixed freezeMs, Fixed dtMs)
{
    return static_cast<int32_t>((freezeMs.raw + dtMs.raw - 1) / dtMs.raw);
}
}  // namespace

// 系统 DebugSinkSystem：11.2 行，见 DESIGN.md §3.2。把事实队列翻译成追踪行。
void runDebugSink(World& world)
{
    world.clearTraceLines();
    auto& ecs        = world.ecs();
    const Fixed dtMs = ecs.singleton<WorldClock>().dtMs;
    for (const CastEvent& event : ecs.singleton<EventQueue<CastEvent>>().items)
    {
        const std::string key = entityKey(world, event.entity);
        if (event.phase == CastPhase::Began)
            world.appendTrace("cast " + key + ' ' + std::to_string(event.skillId) + ' ' + slotLetter(world, event));
        else if (event.phase == CastPhase::Ended)
            world.appendTrace("castend " + key + ' ' + std::to_string(event.skillId));
        else if (event.phase == CastPhase::Cancelled)
            world.appendTrace("castend " + key + ' ' + std::to_string(event.skillId));
    }
    for (const SpawnEvent& event : ecs.singleton<EventQueue<SpawnEvent>>().items)
    {
        const std::string owner = entityKey(world, event.owner);
        const std::string key   = entityKey(world, event.entity);
        const auto& identity    = ecs.get<IdentityComponent>(event.entity);
        if (event.kind == SpawnKind::Effect || event.kind == SpawnKind::Role)
            if (identity.born.frame != 0)
                world.appendTrace("spawn " + owner + ' ' + key);
    }
    for (const HitEvent& event : ecs.singleton<EventQueue<HitEvent>>().items)
    {
        const std::string effect   = entityKey(world, event.effect);
        const std::string target   = entityKey(world, event.target);
        const std::string attacker = entityKey(world, event.attacker);
        world.appendTrace("hit " + effect + ' ' + target + ' ' + std::to_string(event.hitDefId) + ' ' +
                          hitResultName(event.result));
        if (event.result == HitResult::Hit)
            world.appendTrace("dmg " + attacker + ' ' + target + ' ' + std::to_string(event.effectId) + ' ' +
                              std::to_string(event.damage.toInt()) + ' ' + (event.critical ? '1' : '0'));
    }
    for (const HpEvent& event : ecs.singleton<EventQueue<HpEvent>>().items)
    {
        if (event.cause == HpCause::Hit)
            continue;
        const std::string key = entityKey(world, event.entity);
        const char* reason = "other";
        if (event.cause == HpCause::Dot)
            reason = "dot";
        else if (event.cause == HpCause::Heal)
            reason = "heal";
        else if (event.cause == HpCause::Shield)
            reason = "shield";
        world.appendTrace("hp " + key + ' ' + std::to_string(event.delta.toInt()) + ' ' + reason);
    }
    for (const HitStopEvent& event : ecs.singleton<EventQueue<HitStopEvent>>().items)
        world.appendTrace("stop " + entityKey(world, event.entity) + ' ' +
                          std::to_string(stopFrames(event.freezeMs, dtMs)));
    for (const BuffEvent& event : ecs.singleton<EventQueue<BuffEvent>>().items)
    {
        const std::string key = entityKey(world, event.entity);
        if (event.phase == BuffPhase::Added)
            world.appendTrace("buff + " + key + ' ' + std::to_string(event.buffId));
        else if (event.phase == BuffPhase::Removed)
            world.appendTrace("buff - " + key + ' ' + std::to_string(event.buffId));
    }
    for (const DisplacementEvent& event : ecs.singleton<EventQueue<DisplacementEvent>>().items)
    {
        if (!ecs.alive(event.entity))
            continue;
        const std::string key = entityKey(world, event.entity);
        if (event.phase == DispPhase::Began)
            world.appendTrace("disp+ " + key + ' ' + std::to_string(event.curveId));
        else
        {
            const auto& pos = ecs.get<TransformComponent>(event.entity).position;
            world.appendTrace("disp- " + key + ' ' + std::to_string(event.curveId) + ' ' +
                              std::to_string(toMilli(pos.x)) + ' ' + std::to_string(toMilli(pos.y)) + ' ' +
                              std::to_string(toMilli(pos.z)));
        }
    }
    for (const DeathEvent& event : ecs.singleton<EventQueue<DeathEvent>>().items)
        world.appendTrace("death " + entityKey(world, event.entity));
    // 房间流程事实：状态切换、胜负、警告。摘要导出按这些追踪行还原成 roomstate/victory/failure/warning 行。
    for (const RoomEvent& event : ecs.singleton<EventQueue<RoomEvent>>().items)
    {
        switch (event.kind)
        {
        case RoomEventKind::StateChanged:
            world.appendTrace("roomstate " + std::to_string(event.payload));
            break;
        case RoomEventKind::Victory:
            world.appendTrace("victory " + std::to_string(event.payload));
            break;
        case RoomEventKind::Failure:
            world.appendTrace("failure " + std::to_string(event.payload));
            break;
        case RoomEventKind::Warning:
            world.appendTrace("warning " + std::to_string(event.payload));
            break;
        case RoomEventKind::PvpEnded:
            break;
        }
    }
}
}  // namespace mugen::sim
