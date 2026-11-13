#include "SummaryExport.h"

#include "mugen/sim/DrawPolicy.h"
#include "mugen/sim/buff/BuffRules.h"
#include "mugen/sim/room/RoomComponents.h"
#include <map>
#include <sstream>
#include <stdexcept>

namespace mugen::sim_cli
{
using namespace mugen::sim;
namespace
{
// 定点原始值（Q44.20）转毫单位，向下取整。
int64_t toMilli(const Fixed& value)
{
    const int64_t scaled = value.raw * 1000;
    int64_t result       = scaled / Fixed::kOne;
    if (scaled % Fixed::kOne != 0 && scaled < 0)
        --result;
    return result;
}
// 状态位名称（枚举顺序与旧实现一致）。
std::string statusNames(EntityRoleStatus status)
{
    static constexpr std::pair<EntityRoleStatus, const char*> kBits[] = {
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
        if ((static_cast<uint32_t>(status) & static_cast<uint32_t>(bit)) != 0)
        {
            if (!out.empty())
                out += ',';
            out += name;
        }
    return out.empty() ? "-" : out;
}
// 观察到的角色。
struct RoleWatch
{
    std::string key;
    std::string status;
    std::string action;
    int64_t hp = 0;
    int64_t x = 0, y = 0, z = 0;
};
// 观察到的房间实体（障碍 / 物品 / 传送门）：键、种类与当前阶段。
struct RoomEntityWatch
{
    std::string key;
    SpawnKind kind = SpawnKind::Obstacle;
    int32_t phase = 0;
};
// 逐帧观察器。
class Exporter
{
public:
    Exporter(World& world, summary::Summary& out) : m_world(world), m_out(out) {}

    void scanInitial()
    {
        m_world.ecs().each<IdentityComponent, RoleStateComponent, MotionStateComponent>(
            [&](Entity entity, const IdentityComponent& identity, const RoleStateComponent& state, const MotionStateComponent& motion) {
                if (identity.m_kind != SpawnKind::Role)
                    return;
                RoleWatch watch;
                watch.key    = roleKey(identity);
                watch.status = statusNames(role::statusBits(state.m_state));
                watch.action = motion.m_motionName;
                m_roles[entity.value] = watch;
            });
        // 记录房间流程与房间实体的初始基线，避免把装配期实体当成掉落/开启。
        const auto& room  = m_world.ecs().singleton<sim::RoomState>();
        m_wave            = room.m_wave;
        m_usedRevives     = room.m_usedRevives;
        m_ko              = room.m_ko;
        m_roomEntities    = scanRoomEntities();
    }
    void afterTick(int64_t frame)
    {
        for (const std::string& line : m_world.traceLines())
            handleEvent(frame, line);
        scanNewRoles();
        observeRoles(frame);
        observeRoom(frame);
        observeRoomEntities(frame);
    }
    void finish()
    {
        for (const auto& [value, watch] : m_roles)
        {
            summary::SummaryLine line;
            line.type   = "final";
            line.fields = {watch.key, std::to_string(watch.hp), summary::formatMilli(watch.x),
                           summary::formatMilli(watch.y), summary::formatMilli(watch.z), watch.status};
            m_out.lines.push_back(std::move(line));
        }
    }

private:
    static std::string roleKey(const IdentityComponent& identity)
    {
        return "role:" + std::to_string(identity.m_configId) + "#" + std::to_string(identity.m_ordinal);
    }
    // 房间实体语义键：obstacle/goods/portal:<configId>#<n>；按实体句柄缓存，首次出现才分配序号。
    std::string roomEntityKey(Entity entity, const IdentityComponent& identity)
    {
        const auto found = m_roomEntityKeys.find(entity.value);
        if (found != m_roomEntityKeys.end())
            return found->second;
        const char* stem = nullptr;
        switch (identity.m_kind)
        {
        case SpawnKind::Obstacle:
            stem = "obstacle";
            break;
        case SpawnKind::Goods:
            stem = "goods";
            break;
        case SpawnKind::Portal:
            stem = "portal";
            break;
        default:
            return "-";
        }
        const std::string key = std::string(stem) + ':' + std::to_string(identity.m_configId) + '#' +
                                std::to_string(++m_roomCounts[std::string(stem) + ':' +
                                                               std::to_string(identity.m_configId)]);
        m_roomEntityKeys[entity.value] = key;
        return key;
    }
    void emit(int64_t frame, const char* type, std::vector<std::string> fields)
    {
        summary::SummaryLine line;
        line.type   = type;
        line.frame  = frame;
        line.fields = std::move(fields);
        m_out.lines.push_back(std::move(line));
    }
    void handleEvent(int64_t frame, const std::string& text)
    {
        std::istringstream row(text);
        std::vector<std::string> w;
        std::string word;
        while (row >> word)
            w.push_back(word);
        if (w.empty())
            return;
        if (w[0] == "cast" && w.size() >= 4)
            emit(frame, "cast", {w[1], w[2], w[3]});
        else if (w[0] == "castend" && w.size() >= 3)
            emit(frame, "castend", {w[1], w[2], "done"});
        else if (w[0] == "dmg" && w.size() >= 6)
            emit(frame, "dmg", {w[1], w[2], w[3], w[4], w[5]});
        else if (w[0] == "hit" && w.size() >= 5)
            emit(frame, "hit", {w[1], w[2], w[3], w[4]});
        else if (w[0] == "stop" && w.size() >= 3)
            emit(frame, "stop", {w[1], w[2]});
        else if (w[0] == "buff" && w.size() >= 4)
            emit(frame, w[1] == "+" ? "buff+" : "buff-", {w[2], w[3]});
        else if (w[0] == "spawn" && w.size() >= 3)
            emit(frame, "spawn", {w[1], w[2]});
        else if (w[0] == "death" && w.size() >= 2)
            emit(frame, "death", {w[1]});
        else if (w[0] == "hp" && w.size() >= 4)
            emit(frame, "hp", {w[1], w[2], w[3]});
        else if (w[0] == "disp+" && w.size() >= 3)
            emit(frame, "disp+", {w[1], w[2]});
        else if (w[0] == "disp-" && w.size() >= 6)
            emit(frame, "disp-",
                 {w[1], w[2], summary::formatMilli(std::stoll(w[3])), summary::formatMilli(std::stoll(w[4])),
                  summary::formatMilli(std::stoll(w[5]))});
        else if (w[0] == "roomstate" && w.size() >= 2)
            emit(frame, "roomstate", {w[1]});
        else if (w[0] == "victory" && w.size() >= 2)
            emit(frame, "victory", {w[1]});
        else if (w[0] == "failure" && w.size() >= 2)
            emit(frame, "failure", {w[1]});
        else if (w[0] == "warning" && w.size() >= 2)
            emit(frame, "warning", {w[1]});
    }
    // 登记战斗中途新生成的角色（波次、召唤），供状态/动作差分与 final 行追踪。
    // 生成行本身由追踪（spawn）输出；这里只补登记，status/action 留空让 observeRoles 补首拍 state/action 行。
    void scanNewRoles()
    {
        m_world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
            if (identity.m_kind != SpawnKind::Role || m_roles.count(entity.value))
                return;
            RoleWatch watch;
            watch.key = roleKey(identity);
            m_roles[entity.value] = std::move(watch);
        });
    }
    void observeRoles(int64_t frame)
    {
        for (auto& [value, watch] : m_roles)
        {
            const Entity entity{value};
            if (!m_world.ecs().alive(entity) || !m_world.ecs().has<RoleStateComponent>(entity))
                continue;
            const auto status = statusNames(role::statusBits(m_world.ecs().get<RoleStateComponent>(entity).m_state));
            if (status != watch.status)
            {
                emit(frame, "state", {watch.key, watch.status.empty() ? "-" : watch.status, status});
                watch.status = status;
            }
            if (const auto* motion = m_world.ecs().tryGet<MotionStateComponent>(entity))
            {
                if (motion->m_motionName != watch.action)
                {
                    if (!watch.action.empty() && watch.action != "-")
                        emit(frame, "actionend", {watch.key, watch.action});
                    if (!motion->m_motionName.empty() && motion->m_motionName != "-")
                        emit(frame, "action", {watch.key, motion->m_motionName});
                    watch.action = motion->m_motionName;
                }
            }
            const auto& health = m_world.ecs().get<HealthComponent>(entity);
            const auto& pos    = m_world.ecs().get<TransformComponent>(entity).m_position;
            watch.hp           = health.m_hp.toInt();
            watch.x            = toMilli(pos.x);
            watch.y            = toMilli(pos.y);
            watch.z            = toMilli(pos.z);
        }
    }
    // 主控玩家语义键（复活行使用）。
    std::string heroKey()
    {
        const auto& slots = m_world.ecs().singleton<PlayerSlots>().m_entities;
        if (slots.empty())
            return "-";
        const auto& identity = m_world.ecs().get<IdentityComponent>(slots.begin()->second);
        return roleKey(identity);
    }
    // 收集当前存活的障碍 / 物品 / 传送门，返回句柄值到观察结构的映射。
    std::map<uint32_t, RoomEntityWatch> scanRoomEntities()
    {
        std::map<uint32_t, RoomEntityWatch> result;
        m_world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
            RoomEntityWatch watch;
            watch.kind = identity.m_kind;
            switch (identity.m_kind)
            {
            case SpawnKind::Obstacle:
                watch.key   = roomEntityKey(entity, identity);
                watch.phase = 0;
                result[entity.value] = watch;
                break;
            case SpawnKind::Goods:
                watch.key   = roomEntityKey(entity, identity);
                watch.phase = static_cast<int32_t>(m_world.ecs().get<GoodsStateComponent>(entity).m_phase);
                result[entity.value] = watch;
                break;
            case SpawnKind::Portal:
                watch.key   = roomEntityKey(entity, identity);
                watch.phase = static_cast<int32_t>(m_world.ecs().get<PortalStateComponent>(entity).m_phase);
                result[entity.value] = watch;
                break;
            default:
                break;
            }
        });
        return result;
    }
    // 房间单例差分：波次、复活、KO。
    void observeRoom(int64_t frame)
    {
        const auto& room = m_world.ecs().singleton<sim::RoomState>();
        for (int32_t wave = m_wave + 1; wave <= room.m_wave; ++wave)
            emit(frame, "wave", {std::to_string(wave)});
        m_wave = room.m_wave;
        if (room.m_usedRevives > m_usedRevives)
            emit(frame, "revive", {heroKey()});
        m_usedRevives = room.m_usedRevives;
        if (room.m_ko && !m_ko)
            emit(frame, "ko", {std::to_string(room.m_roomId)});
        m_ko = room.m_ko;
    }
    // 房间实体差分：物品掉落 / 拾取、传送门开启 / 传送、障碍击破。
    void observeRoomEntities(int64_t frame)
    {
        const auto current = scanRoomEntities();
        for (const auto& [value, watch] : current)
        {
            const auto found = m_roomEntities.find(value);
            if (found == m_roomEntities.end())
            {
                if (watch.kind == SpawnKind::Goods)
                    emit(frame, "goods", {watch.key, "drop"});
                m_roomEntities[value] = watch;
                continue;
            }
            auto& prev = found->second;
            if (watch.kind == SpawnKind::Goods)
            {
                if (watch.phase != prev.phase && watch.phase == static_cast<int32_t>(GoodsPhase::Collect))
                    emit(frame, "goods", {watch.key, "pickup"});
            }
            else if (watch.kind == SpawnKind::Portal)
            {
                if (watch.phase != prev.phase)
                {
                    if (watch.phase == static_cast<int32_t>(PortalPhase::Active))
                        emit(frame, "portal", {watch.key, "open"});
                    else if (watch.phase == static_cast<int32_t>(PortalPhase::Transfer))
                        emit(frame, "portal", {watch.key, "transfer"});
                }
            }
            prev.phase = watch.phase;
        }
        for (auto it = m_roomEntities.begin(); it != m_roomEntities.end();)
        {
            if (current.find(it->first) != current.end())
            {
                ++it;
                continue;
            }
            if (it->second.kind == SpawnKind::Obstacle)
                emit(frame, "obstacle", {it->second.key, "break"});
            it = m_roomEntities.erase(it);
        }
    }

    World& m_world;
    summary::Summary& m_out;
    std::map<uint32_t, RoleWatch> m_roles;
    // 上一帧的波次、复活与 KO 基线。
    int32_t m_wave = 0;
    int32_t m_usedRevives = 0;
    bool m_ko = false;
    // 房间实体按配置的生成序号。
    std::map<std::string, int> m_roomCounts;
    // 房间实体句柄值 → 已分配的语义键。
    std::map<uint32_t, std::string> m_roomEntityKeys;
    // 上一帧存活房间实体。
    std::map<uint32_t, RoomEntityWatch> m_roomEntities;
};
}  // namespace

// 在该帧 tick 之前按脚本顺序准入 Buff。目标按当下创建顺序解析。
void applyBuffs(World& world, uint64_t frame, const std::vector<BuffInject>& buffs)
{
    bool any = false;
    for (const auto& row : buffs)
        if (row.frame == frame)
        {
            any = true;
            break;
        }
    if (!any)
        return;
    Entity hero;
    std::vector<Entity> dummies;
    world.ecs().each<IdentityComponent>([&](Entity entity, const IdentityComponent& identity) {
        if (identity.m_kind != SpawnKind::Role)
            return;
        if (identity.m_playerSlot && *identity.m_playerSlot == 0)
            hero = entity;
        else if (!identity.m_playerSlot && identity.m_owner.value == 0)
            dummies.push_back(entity);
    });
    if (hero.value == 0)
        throw std::runtime_error("Buff inject: hero is missing");
    auto admitOne = [&](Entity target, int32_t buffId) {
        if (target.value == 0 || !world.ecs().alive(target))
            throw std::runtime_error("Buff inject: target is missing at frame " + std::to_string(frame));
        BuffContext ctx;
        ctx.m_applicator = hero;
        buff::rules::admit(world, target, buffId, ctx);
    };
    for (const auto& row : buffs)
    {
        if (row.frame != frame)
            continue;
        if (row.target == "hero")
            admitOne(hero, row.buffId);
        else if (row.target == "dummy1")
            admitOne(dummies.empty() ? Entity{} : dummies[0], row.buffId);
        else if (row.target == "dummy2")
            admitOne(dummies.size() < 2 ? Entity{} : dummies[1], row.buffId);
        else if (row.target == "all-dummies")
        {
            if (dummies.empty())
                throw std::runtime_error("Buff inject: target is missing at frame " + std::to_string(frame));
            for (Entity dummy : dummies)
                admitOne(dummy, row.buffId);
        }
        else
            throw std::runtime_error("Buff inject: unknown target " + row.target);
    }
}

summary::Summary exportSummary(World& world, const std::map<uint64_t, FrameInput>& inputs,
                               const std::vector<BuffInject>& buffs, const ExportParams& params)
{
    if (params.crit != "min" && params.crit != "max")
        throw std::invalid_argument("--crit must be min or max");
    DrawPolicy policy;
    policy.m_crit = Fixed::fromInt(params.crit == "min" ? 101 : 0);
    world.setDrawPolicy(&policy);

    summary::Summary result;
    result.header.scenarioInfo = params.scenario + " room " + std::to_string(params.room) + " seed " +
                                 std::to_string(params.seed) + " ai off";
    result.header.drawInfo = "crit=" + params.crit + " float=mid dodge=never prob=pass choice=first";

    Exporter exporter(world, result);
    exporter.scanInitial();
    for (uint64_t frame = 1; frame <= params.frames; ++frame)
    {
        applyBuffs(world, frame, buffs);
        const auto input = inputs.find(frame);
        world.tick(input == inputs.end() ? FrameInput{frame, {}, {}, {}} : input->second);
        exporter.afterTick(static_cast<int64_t>(frame));
    }
    exporter.finish();
    world.setDrawPolicy(nullptr);
    summary::sortSummary(result);
    return result;
}
}  // namespace mugen::sim_cli
