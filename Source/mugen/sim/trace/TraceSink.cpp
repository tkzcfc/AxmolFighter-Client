#include "TraceSink.h"
#include "../Components.h"
#include "../Singletons.h"
#include "../World.h"
#include "../buff/BuffComponents.h"
#include "../effect/Effect.h"
#include "../event/PresentationEvent.h"
#include "../room/RoomEntities.h"
#include "../role/RoleComponents.h"
#include "../skill/SkillComponents.h"
#include "mugen/conf/GameDef.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace mugen::sim::trace
{
namespace
{
// 将定点原始整数转为十进制字符串，保证跨平台稳定。
std::string fixedStr(const Fixed& value)
{
    return std::to_string(value.raw);
}

std::string groupName(GroupKind kind)
{
    switch (kind)
    {
    case GroupKind::Role: return "role";
    case GroupKind::Effect: return "effect";
    case GroupKind::Obstacle: return "obstacle";
    case GroupKind::Goods: return "goods";
    case GroupKind::Portal: return "portal";
    default: return "?";
    }
}

// 按枚举定义顺序输出命中的状态位名称。
std::string statusNames(EntityRoleStatus status)
{
    static constexpr std::pair<EntityRoleStatus, std::string_view> kBits[] = {
        {EntityRoleStatus::Birth, "Birth"},          {EntityRoleStatus::Death, "Death"},
        {EntityRoleStatus::Wake, "Wake"},            {EntityRoleStatus::GetUp, "GetUp"},
        {EntityRoleStatus::HitFloor, "HitFloor"},    {EntityRoleStatus::HitDown, "HitDown"},
        {EntityRoleStatus::HitUp, "HitUp"},          {EntityRoleStatus::HitSwitch, "HitSwitch"},
        {EntityRoleStatus::Hit, "Hit"},              {EntityRoleStatus::Attack, "Attack"},
        {EntityRoleStatus::Fall, "Fall"},            {EntityRoleStatus::Patrol, "Patrol"},
        {EntityRoleStatus::Chase, "Chase"},          {EntityRoleStatus::Alert, "Alert"},
        {EntityRoleStatus::Run, "Run"},              {EntityRoleStatus::Walk, "Walk"},
        {EntityRoleStatus::Idle, "Idle"},            {EntityRoleStatus::Finding, "Finding"},
        {EntityRoleStatus::Jostled, "Jostled"},      {EntityRoleStatus::Revive, "Revive"},
        {EntityRoleStatus::Controlled, "Controlled"},
    };
    std::string out;
    for (const auto& [bit, name] : kBits)
    {
        if ((static_cast<uint32_t>(status) & static_cast<uint32_t>(bit)) != 0)
        {
            if (!out.empty())
                out += ',';
            out += name;
        }
    }
    return out;
}

// 输出当前生效的额外控制状态名称。
std::string extraStateNames(const ExtraState& extra)
{
    static constexpr std::pair<mugen::ExtraState, std::string_view> kStates[] = {
        {mugen::ExtraState::Durance, "Durance"},       {mugen::ExtraState::Palsy, "Palsy"},
        {mugen::ExtraState::Stun, "Stun"},             {mugen::ExtraState::Silence, "Silence"},
        {mugen::ExtraState::SuperArmor, "SuperArmor"}, {mugen::ExtraState::Invincible, "Invincible"},
        {mugen::ExtraState::Stealth, "Stealth"},       {mugen::ExtraState::Frozen, "Frozen"},
    };
    std::string out;
    for (const auto& [state, name] : kStates)
    {
        if (extra.active(state))
        {
            if (!out.empty())
                out += ',';
            out += name;
        }
    }
    return out;
}

// 按实体组读取其语义主键（角色配置/特效配置/障碍/掉落/传送门标识）。
int32_t entityId(const World& world, Entity entity, GroupKind kind)
{
    const auto& ecs = world.ecs();
    switch (kind)
    {
    case GroupKind::Role:
        if (const auto* id = ecs.tryGet<RoleIdentity>(entity))
            return id->roleId;
        break;
    case GroupKind::Effect:
        if (const auto* state = ecs.tryGet<EffectState>(entity))
            return state->m_configId;
        break;
    case GroupKind::Obstacle:
        if (const auto* state = ecs.tryGet<ObstacleState>(entity))
            return state->m_id;
        break;
    case GroupKind::Goods:
        if (const auto* state = ecs.tryGet<GoodsState>(entity))
            return state->m_id;
        break;
    case GroupKind::Portal:
        if (const auto* state = ecs.tryGet<PortalState>(entity))
            return state->m_id;
        break;
    default: break;
    }
    return 0;
}

// 输出稳定实体键：组名.主键.生成序号；已销毁实体回退为句柄。
std::string entityKey(const World& world, Entity entity)
{
    const auto* group = world.ecs().tryGet<EntityGroup>(entity);
    if (!group)
        return "gone." + std::to_string(entity.index()) + "." + std::to_string(entity.generation());
    return std::string(groupName(group->kind)) + "." + std::to_string(entityId(world, entity, group->kind)) + "." +
           std::to_string(world.spawnSeq(entity));
}

// 输出单个实体的语义状态行。
void emitEntity(TraceSink& sink, const World& world, Entity entity, uint64_t frame)
{
    const auto& ecs     = world.ecs();
    const auto* group   = ecs.tryGet<EntityGroup>(entity);
    if (!group)
        return;
    std::string line    = "ent " + std::to_string(frame) + ' ' + groupName(group->kind) + ' ' +
                       std::to_string(entityId(world, entity, group->kind)) + ' ' +
                       std::to_string(world.spawnSeq(entity));
    if (const auto* t = ecs.tryGet<Transform>(entity))
    {
        line += " x " + fixedStr(t->position.x);
        line += " y " + fixedStr(t->position.y);
        line += " z " + fixedStr(t->position.z);
        line += " face " + std::to_string(t->towardX);
    }
    if (const auto* v = ecs.tryGet<Vitals>(entity))
    {
        line += " hp " + fixedStr(v->hp);
        line += " mp " + fixedStr(v->mp);
        line += " ep " + fixedStr(v->ep);
    }
    if (const auto* s = ecs.tryGet<RoleStatus>(entity))
    {
        const std::string names = statusNames(s->eEntityStatus);
        line += " st " + (names.empty() ? std::string("-") : names);
    }
    if (const auto* extra = ecs.tryGet<ExtraState>(entity))
    {
        const std::string names = extraStateNames(*extra);
        line += " ex " + (names.empty() ? std::string("-") : names);
    }
    if (const auto* m = ecs.tryGet<MotionState>(entity))
    {
        line += " act " + (m->motionName.empty() ? std::string("-") : m->motionName);
        line += " at " + fixedStr(m->timeMs);
        line += " play " + std::to_string(m->playing ? 1 : 0);
    }
    if (const auto* pool = ecs.tryGet<SkillPool>(entity))
    {
        if (pool->m_current != kNoSkill && pool->m_current >= 0 &&
            static_cast<size_t>(pool->m_current) < pool->m_skills.size())
        {
            const auto& base = pool->m_skills[static_cast<size_t>(pool->m_current)];
            const std::string slot =
                base.m_address.m_slot < pool->m_slots.size() ? pool->m_slots[base.m_address.m_slot].m_name : "?";
            line += " sk " + slot;
            line += ' ' + std::to_string(base.m_address.m_index);
            line += ' ' + std::to_string(base.m_address.m_mode);
            line += ' ' + std::to_string(base.m_address.m_step);
            line += ' ' + std::to_string(base.m_configId);
        }
        else
            line += " sk - - - - -";
    }
    if (const auto* buffs = ecs.tryGet<BuffPool>(entity))
    {
        line += " buf";
        for (const auto& instance : buffs->m_instances)
            line += ' ' + std::to_string(instance.m_initialId);
        if (buffs->m_instances.empty())
            line += " -";
    }
    sink.write(line);
}

// 输出单个表现事件行，行为级事件按载荷种类命名。
void emitEvent(TraceSink& sink, const World& world, const PresentationEvent& event)
{
    std::string line    = "ev " + std::to_string(event.frame) + ' ' + std::to_string(event.seq);
    const std::string key = entityKey(world, event.entity);
    std::visit(
        [&](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, Sound>)
                line += " sound " + key + " id " + std::to_string(payload.id);
            else if constexpr (std::is_same_v<T, CameraShake>)
                line += " shake " + key + " d " + fixedStr(payload.durationMs);
            else if constexpr (std::is_same_v<T, DisplaySpine>)
                line += " spine " + key + " id " + std::to_string(payload.id);
            else if constexpr (std::is_same_v<T, HurtNumber>)
                line += " damage " + key + " value " + std::to_string(payload.value);
            else if constexpr (std::is_same_v<T, Ghost>)
                line += " ghost " + key;
            else if constexpr (std::is_same_v<T, Bubble>)
                line += " bubble " + key + " id " + std::to_string(payload.id);
            else if constexpr (std::is_same_v<T, Tips>)
                line += " tips " + key + " text " + payload.text;
            else if constexpr (std::is_same_v<T, RoomStateChanged>)
                line += " roomstate " + std::to_string(payload.state);
            else if constexpr (std::is_same_v<T, Victory>)
                line += " victory room " + std::to_string(payload.roomId);
            else if constexpr (std::is_same_v<T, Failure>)
                line += " failure room " + std::to_string(payload.roomId);
            else if constexpr (std::is_same_v<T, RoleDeath>)
                line += " death " + key + " role " + std::to_string(payload.m_roleId);
            else if constexpr (std::is_same_v<T, RoleIntensity>)
                line += " intensity " + key + " v " + fixedStr(payload.m_value);
            else if constexpr (std::is_same_v<T, WhiteFrame>)
                line += " whiteframe " + key + " on " + std::to_string(payload.m_enabled ? 1 : 0);
            else if constexpr (std::is_same_v<T, BuffSpine>)
                line += " buff " + key + " serial " + std::to_string(payload.m_serial) + " spine " +
                        std::to_string(payload.m_spineId) + " stage " + std::to_string(payload.m_stage) + " buff " +
                        std::to_string(payload.m_buffId);
            else if constexpr (std::is_same_v<T, RoomWarning>)
                line += " warn id " + std::to_string(payload.m_id) + " boss " +
                        std::to_string(payload.m_boss ? 1 : 0);
        },
        event.payload);
    sink.write(line);
}
}  // namespace

void TraceSink::write(std::string_view line)
{
    m_text.append(line);
    m_text.push_back('\n');
}

void emitFrame(TraceSink& sink, World& world)
{
    const uint64_t frame = world.ecs().singleton<WorldClock>().frame;
    sink.write("frame " + std::to_string(frame));
    world.ecs().each<EntityGroup>([&](Entity entity, const EntityGroup&) { emitEntity(sink, world, entity, frame); });
    for (const auto& event : world.ecs().singleton<PresentationEvents>().events)
        emitEvent(sink, world, event);
}

namespace
{
// 未挂载追踪时不拼接文本，避免战斗热路径产生额外开销。
bool tracing(const World& world) { return world.trace() != nullptr; }

void writeEvent(World& world, const std::string& line)
{
    world.trace()->write(line);
}
}  // namespace

void noteCast(World& world, Entity entity, int32_t configId, std::string_view slot, int32_t pipe)
{
    if (!tracing(world))
        return;
    const auto name = slot.empty() ? std::string_view{"-"} : slot;
    writeEvent(world, "cast " + entityKey(world, entity) + ' ' + std::to_string(configId) + ' ' + std::string(name) +
                          ' ' + std::to_string(pipe));
}

void noteCastEnd(World& world, Entity entity, int32_t configId)
{
    if (!tracing(world))
        return;
    writeEvent(world, "castend " + entityKey(world, entity) + ' ' + std::to_string(configId));
}

void noteDamage(World& world, Entity attacker, Entity target, int32_t sourceConfigId, int64_t value, bool critical)
{
    if (!tracing(world))
        return;
    writeEvent(world, "dmg " + entityKey(world, attacker) + ' ' + entityKey(world, target) + ' ' +
                          std::to_string(sourceConfigId) + ' ' + std::to_string(value) + ' ' +
                          std::to_string(critical ? 1 : 0));
}

void noteBuffAdd(World& world, Entity entity, int32_t buffId)
{
    if (!tracing(world))
        return;
    writeEvent(world, "buff + " + entityKey(world, entity) + ' ' + std::to_string(buffId));
}

void noteBuffRemove(World& world, Entity entity, int32_t buffId)
{
    if (!tracing(world))
        return;
    writeEvent(world, "buff - " + entityKey(world, entity) + ' ' + std::to_string(buffId));
}

void noteSpawn(World& world, Entity owner, int32_t effectConfigId)
{
    if (!tracing(world))
        return;
    writeEvent(world, "spawn " + entityKey(world, owner) + ' ' + std::to_string(effectConfigId));
}

void noteDeath(World& world, Entity entity)
{
    if (!tracing(world))
        return;
    writeEvent(world, "death " + entityKey(world, entity));
}
}  // namespace mugen::sim::trace
