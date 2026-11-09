// [11.2] DebugSink
// 读：全部事实队列
// 写：外部追踪行（World::appendTrace）
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
}  // namespace

// 系统 DebugSink：11.2 行，见 DESIGN.md §3.2。把事实队列翻译成追踪行。
void runDebugSink(World& world)
{
    auto& ecs = world.ecs();
    for (const CastEvent& event : ecs.singleton<EventQueue<CastEvent>>().m_items)
    {
        const std::string key = entityKey(world, event.m_entity);
        if (event.m_phase == CastPhase::Began)
            world.appendTrace("cast " + key + ' ' + std::to_string(event.m_skillId) + ' ' + event.m_slot);
        else if (event.m_phase == CastPhase::Ended)
            world.appendTrace("castend " + key + ' ' + std::to_string(event.m_skillId));
        else if (event.m_phase == CastPhase::Cancelled)
            world.appendTrace("castend " + key + ' ' + std::to_string(event.m_skillId));
    }
    for (const SpawnEvent& event : ecs.singleton<EventQueue<SpawnEvent>>().m_items)
    {
        const std::string owner = entityKey(world, event.m_owner);
        const std::string key   = entityKey(world, event.m_entity);
        // 初始出生（帧 0）不宣告，仅运行期生成的实体产出 spawn 行。
        const auto& identity = ecs.get<Identity>(event.m_entity);
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
        world.appendTrace("dmg " + attacker + ' ' + target + ' ' + std::to_string(event.m_effectId) + ' ' +
                          std::to_string(event.m_damage.toInt()) + ' ' + (event.m_critical ? '1' : '0'));
    }
    for (const HpEvent& event : ecs.singleton<EventQueue<HpEvent>>().m_items)
    {
        if (event.m_cause == HpCause::Hit)
            continue;
        const std::string key = entityKey(world, event.m_entity);
        world.appendTrace("hp " + key + ' ' + std::to_string(event.m_delta.toInt()) + ' ' +
                          (event.m_cause == HpCause::Dot ? "dot" : event.m_cause == HpCause::Heal ? "heal" : "other"));
    }
    for (const StopEvent& event : ecs.singleton<EventQueue<StopEvent>>().m_items)
        world.appendTrace("stop " + entityKey(world, event.m_entity) + ' ' + std::to_string(event.m_frames));
    for (const BuffEvent& event : ecs.singleton<EventQueue<BuffEvent>>().m_items)
    {
        const std::string key = entityKey(world, event.m_entity);
        if (event.m_phase == BuffPhase::Added)
            world.appendTrace("buff + " + key + ' ' + std::to_string(event.m_buffId));
        else if (event.m_phase == BuffPhase::Removed)
            world.appendTrace("buff - " + key + ' ' + std::to_string(event.m_buffId));
    }
    for (const DispEvent& event : ecs.singleton<EventQueue<DispEvent>>().m_items)
    {
        const std::string key = entityKey(world, event.m_entity);
        if (event.m_phase == 0)
            world.appendTrace("disp+ " + key + ' ' + std::to_string(event.m_displacementId));
        else
            world.appendTrace("disp- " + key + ' ' + std::to_string(event.m_displacementId) + ' ' +
                              std::to_string(toMilli(event.m_endpoint.x)) + ' ' +
                              std::to_string(toMilli(event.m_endpoint.y)) + ' ' +
                              std::to_string(toMilli(event.m_endpoint.z)));
    }
    for (const DeathEvent& event : ecs.singleton<EventQueue<DeathEvent>>().m_items)
        world.appendTrace("death " + entityKey(world, event.m_entity));
}
}  // namespace mugen::sim
