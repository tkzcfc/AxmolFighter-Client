#include "SummaryExport.h"

#include "mugen/sim_legacy/DrawPolicy.h"
#include "mugen/sim_legacy/trace/TraceSink.h"
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mugen::legacy_cli
{
using namespace mugen::sim_legacy;
namespace
{
// 定点原始值（Q44.20）转毫单位，向下取整，保证跨平台稳定。
int64_t toMilli(const Fixed& value)
{
    const int64_t scaled = value.raw * 1000;
    int64_t result       = scaled / Fixed::kOne;
    if (scaled % Fixed::kOne != 0 && scaled < 0)
        --result;
    return result;
}

// 按枚举定义顺序列出命中的状态位名称，无则 `-`。
std::string statusNames(EntityRoleStatus status)
{
    static constexpr std::pair<EntityRoleStatus, const char*> kBits[] = {
        {EntityRoleStatus::Birth, "Birth"},     {EntityRoleStatus::Death, "Death"},
        {EntityRoleStatus::Wake, "Wake"},       {EntityRoleStatus::GetUp, "GetUp"},
        {EntityRoleStatus::HitFloor, "HitFloor"}, {EntityRoleStatus::HitDown, "HitDown"},
        {EntityRoleStatus::HitUp, "HitUp"},     {EntityRoleStatus::HitSwitch, "HitSwitch"},
        {EntityRoleStatus::Hit, "Hit"},         {EntityRoleStatus::Attack, "Attack"},
        {EntityRoleStatus::Fall, "Fall"},       {EntityRoleStatus::Patrol, "Patrol"},
        {EntityRoleStatus::Chase, "Chase"},     {EntityRoleStatus::Alert, "Alert"},
        {EntityRoleStatus::Run, "Run"},         {EntityRoleStatus::Walk, "Walk"},
        {EntityRoleStatus::Idle, "Idle"},       {EntityRoleStatus::Finding, "Finding"},
        {EntityRoleStatus::Jostled, "Jostled"}, {EntityRoleStatus::Revive, "Revive"},
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

const char* groupName(GroupKind kind)
{
    switch (kind)
    {
    case GroupKind::Role:
        return "role";
    case GroupKind::Effect:
        return "effect";
    case GroupKind::Obstacle:
        return "obstacle";
    case GroupKind::Goods:
        return "goods";
    default:
        return "portal";
    }
}

// 观察到的角色：上一帧的值与最后已知值。
struct RoleWatch
{
    std::string key;
    std::string status;
    std::string action;
    // 最后已知的生命、位置（毫单位）与状态，实体销毁后仍用于 final 行。
    int64_t hp = 0;
    int64_t x = 0, y = 0, z = 0;
    bool alive = true;
};

// 观察到的位移：从 disp+ 起到速度与加速度耗尽。
struct DispWatch
{
    std::string key;
    int32_t id = 0;
    int64_t x = 0, y = 0, z = 0;
};

// 逐帧观察器：把追踪事件行与组件状态差分翻译成摘要行。
class Exporter
{
public:
    Exporter(World& world, summary::Summary& out) : m_world(world), m_out(out) {}

    // 记录推进前已存在的实体，不产生 spawn 行。
    void scanInitial() { scanRoles(0, false); }
    // 处理一帧结束后的追踪文本与组件状态。
    void afterTick(int64_t frame, const std::string& traceText)
    {
        scanRoles(frame, true);
        std::istringstream in(traceText);
        std::string line;
        while (std::getline(in, line))
            handleEvent(frame, line);
        observeRoles(frame);
        observeDisplacements(frame);
    }
    // 追加 final 行。
    void finish()
    {
        for (const auto& [entity, watch] : m_roles)
        {
            summary::SummaryLine line;
            line.type   = "final";
            line.fields = {watch.key, std::to_string(watch.hp), summary::formatMilli(watch.x),
                           summary::formatMilli(watch.y), summary::formatMilli(watch.z), watch.status};
            m_out.lines.push_back(std::move(line));
        }
    }

private:
    // 追加一行带帧摘要。
    void emit(int64_t frame, const char* type, std::vector<std::string> fields)
    {
        summary::SummaryLine line;
        line.type   = type;
        line.frame  = frame;
        line.fields = std::move(fields);
        m_out.lines.push_back(std::move(line));
    }
    // 原始追踪键（组名.主键.生成序号）映射为摘要实体键；首次出现时按配置 id 编号。
    std::string mapKey(const std::string& raw)
    {
        const auto found = m_keys.find(raw);
        if (found != m_keys.end())
            return found->second;
        const auto d1 = raw.find('.');
        const auto d2 = raw.find('.', d1 + 1);
        if (d1 == std::string::npos || d2 == std::string::npos)
            return m_keys[raw] = raw;
        const std::string group = raw.substr(0, d1);
        const std::string id    = raw.substr(d1 + 1, d2 - d1 - 1);
        const std::string stem  = (group == "effect" ? std::string("fx") : group) + ':' + id;
        return m_keys[raw] = stem + '#' + std::to_string(++m_counts[stem]);
    }
    // 实体的原始追踪键。
    std::string rawKey(Entity entity, const EntityGroup& group)
    {
        int32_t id = 0;
        auto& ecs  = m_world.ecs();
        if (group.kind == GroupKind::Role)
            id = ecs.get<RoleIdentity>(entity).roleId;
        else if (const auto* effect = ecs.tryGet<EffectState>(entity); effect && group.kind == GroupKind::Effect)
            id = effect->m_configId;
        return std::string(groupName(group.kind)) + '.' + std::to_string(id) + '.' +
               std::to_string(m_world.spawnSeq(entity));
    }
    // 扫描存活实体：登记新角色（可选输出 spawn 行）并刷新原始键到实体的映射。
    void scanRoles(int64_t frame, bool announce)
    {
        auto& ecs = m_world.ecs();
        m_live.clear();
        ecs.each<EntityGroup>([&](Entity entity, const EntityGroup& group) {
            const auto raw = rawKey(entity, group);
            m_live[raw]    = entity;
            if (group.kind != GroupKind::Role || m_roles.count(entity.value))
                return;
            RoleWatch watch;
            watch.key = mapKey(raw);
            if (announce)
                emit(frame, "spawn", {"-", watch.key});
            else
            {
                watch.status = statusNames(ecs.get<RoleStatus>(entity).eEntityStatus);
                if (const auto* motion = ecs.tryGet<MotionState>(entity))
                    watch.action = motion->motionName;
            }
            m_roles[entity.value] = std::move(watch);
        });
    }
    // 取已知实体对应的摘要键；追踪键未映射时按首次出现编号。
    std::string key(const std::string& raw) { return mapKey(raw); }
    // 翻译一行追踪事件。
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
            emit(frame, "cast", {key(w[1]), w[2], w[3]});
        else if (w[0] == "castend" && w.size() >= 3)
            emit(frame, "castend", {key(w[1]), w[2], castEndReason(w[1])});
        else if (w[0] == "dmg" && w.size() >= 6)
            emit(frame, "dmg", {key(w[1]), key(w[2]), w[3], w[4], w[5]});
        else if (w[0] == "buff" && w.size() >= 4)
            emit(frame, w[1] == "+" ? "buff+" : "buff-", {key(w[2]), w[3]});
        else if (w[0] == "death" && w.size() >= 2)
            emit(frame, "death", {key(w[1])});
        else if (w[0] == "x" && w.size() >= 2)
        {
            if (w[1] == "spawn" && w.size() >= 4)
                emit(frame, "spawn", {key(w[2]), key(w[3])});
            else if (w[1] == "hit" && w.size() >= 6)
                emit(frame, "hit", {key(w[2]), key(w[3]), w[4], w[5]});
            else if (w[1] == "stop" && w.size() >= 4)
                emit(frame, "stop", {key(w[2]), w[3]});
            else if (w[1] == "hp" && w.size() >= 5)
                emit(frame, "hp", {key(w[2]), w[3], w[4]});
            else if (w[1] == "disp+" && w.size() >= 4)
            {
                const auto live = m_live.find(w[2]);
                if (live != m_live.end())
                {
                    // 新位移覆盖进行中的位移：先以当前位置结束旧位移。
                    const auto old = m_disps.find(live->second.value);
                    if (old != m_disps.end())
                        endDisplacement(frame, old->second, live->second);
                    m_disps[live->second.value] = {key(w[2]), std::stoi(w[3]), 0, 0, 0};
                }
                emit(frame, "disp+", {key(w[2]), w[3]});
            }
        }
    }
    // 施法结束原因：施法者此刻处于受击或死亡状态视为 interrupt，否则 done；cancel 无法从外部区分。
    std::string castEndReason(const std::string& raw)
    {
        const auto live = m_live.find(raw);
        if (live == m_live.end())
            return "interrupt";
        const auto status = m_world.ecs().get<RoleStatus>(live->second).eEntityStatus;
        const bool hurt   = (static_cast<uint32_t>(status) & (static_cast<uint32_t>(EntityRoleStatus::Hit) |
                                                              static_cast<uint32_t>(EntityRoleStatus::Death))) != 0;
        return hurt ? "interrupt" : "done";
    }
    // 对每个角色差分状态、动作并刷新最后已知值；销毁的角色保留最后值。
    void observeRoles(int64_t frame)
    {
        auto& ecs = m_world.ecs();
        for (auto& [value, watch] : m_roles)
        {
            const Entity entity{value};
            if (!watch.alive)
                continue;
            if (!ecs.alive(entity) || !ecs.has<RoleStatus>(entity))
            {
                watch.alive = false;
                continue;
            }
            const auto status = statusNames(ecs.get<RoleStatus>(entity).eEntityStatus);
            if (status != watch.status)
            {
                emit(frame, "state", {watch.key, watch.status.empty() ? "-" : watch.status, status});
                watch.status = status;
            }
            if (const auto* motion = ecs.tryGet<MotionState>(entity))
            {
                if (motion->motionName != watch.action)
                {
                    if (!watch.action.empty() && watch.action != "-")
                        emit(frame, "actionend", {watch.key, watch.action});
                    if (!motion->motionName.empty() && motion->motionName != "-")
                        emit(frame, "action", {watch.key, motion->motionName});
                    watch.action = motion->motionName;
                }
            }
            const auto& vitals = ecs.get<Vitals>(entity);
            const auto& pos    = ecs.get<Transform>(entity).position;
            watch.hp           = vitals.hp.toInt();
            watch.x            = toMilli(pos.x);
            watch.y            = toMilli(pos.y);
            watch.z            = toMilli(pos.z);
        }
    }
    // 写 disp- 行，终点取当前位置（实体仍存活时）。
    void endDisplacement(int64_t frame, DispWatch& watch, Entity entity)
    {
        if (m_world.ecs().alive(entity))
        {
            const auto& pos = m_world.ecs().get<Transform>(entity).position;
            watch.x         = toMilli(pos.x);
            watch.y         = toMilli(pos.y);
            watch.z         = toMilli(pos.z);
        }
        emit(frame, "disp-",
             {watch.key, std::to_string(watch.id), summary::formatMilli(watch.x), summary::formatMilli(watch.y),
              summary::formatMilli(watch.z)});
    }
    // 位移结束判定：速度与剩余加速度均耗尽（或实体已销毁）时写 disp-；终点取当时位置。
    void observeDisplacements(int64_t frame)
    {
        auto& ecs = m_world.ecs();
        for (auto it = m_disps.begin(); it != m_disps.end();)
        {
            auto& watch      = it->second;
            const Entity ent{it->first};
            bool ended       = !ecs.alive(ent) || !ecs.has<Displacement>(ent);
            if (!ended)
            {
                const auto& pos = ecs.get<Transform>(ent).position;
                watch.x         = toMilli(pos.x);
                watch.y         = toMilli(pos.y);
                watch.z         = toMilli(pos.z);
                const auto& v   = ecs.get<RoleHitState>(ent).forceVelocity;
                const auto& d   = ecs.get<Displacement>(ent);
                const bool moving = v.x != Fixed{} || v.y != Fixed{} || v.z != Fixed{};
                const bool accelerating =
                    (d.acceleration.x != Fixed{} && d.elapsedMs < d.accelerationTime.x) ||
                    (d.acceleration.y != Fixed{} && d.elapsedMs < d.accelerationTime.y) ||
                    (d.acceleration.z != Fixed{} && d.elapsedMs < d.accelerationTime.z);
                ended = !moving && !accelerating;
            }
            if (ended)
            {
                emit(frame, "disp-",
                     {watch.key, std::to_string(watch.id), summary::formatMilli(watch.x),
                      summary::formatMilli(watch.y), summary::formatMilli(watch.z)});
                it = m_disps.erase(it);
            }
            else
                ++it;
        }
    }

    World& m_world;
    summary::Summary& m_out;
    // 原始追踪键 → 摘要键。
    std::map<std::string, std::string> m_keys;
    // 摘要键前缀 → 已分配个数。
    std::map<std::string, int> m_counts;
    // 本帧末存活实体的原始键 → 句柄。
    std::map<std::string, Entity> m_live;
    // 角色句柄值 → 观察状态（按句柄值有序，输出稳定）。
    std::map<uint32_t, RoleWatch> m_roles;
    // 进行中的位移。
    std::map<uint32_t, DispWatch> m_disps;
};
}  // namespace

summary::Summary exportSummary(World& world, const std::map<uint64_t, FrameInput>& inputs, const ExportParams& params)
{
    if (params.crit != "min" && params.crit != "max")
        throw std::invalid_argument("--crit must be min or max");
    DrawPolicy policy;
    policy.crit = Fixed::fromInt(params.crit == "min" ? 101 : 0);
    world.setAiEnabled(false);
    world.setDrawPolicy(&policy);
    trace::TraceSink sink;
    sink.setExtended(true);
    world.setTrace(&sink);

    summary::Summary result;
    result.header.scenarioInfo = params.scenario + " room " + std::to_string(params.room) + " seed " +
                                 std::to_string(params.seed) + " ai off";
    result.header.drawInfo = "crit=" + params.crit + " float=mid dodge=never prob=pass choice=first";

    Exporter exporter(world, result);
    exporter.scanInitial();
    for (uint64_t frame = 1; frame <= params.frames; ++frame)
    {
        const auto input = inputs.find(frame);
        world.tick(input == inputs.end() ? FrameInput{frame, {}, {}, {}} : input->second);
        exporter.afterTick(static_cast<int64_t>(frame), sink.text());
        sink.clear();
    }
    exporter.finish();
    world.setTrace(nullptr);
    world.setDrawPolicy(nullptr);
    summary::sortSummary(result);
    return result;
}
}  // namespace mugen::legacy_cli
