#include "SummaryExport.h"

#include "mugen/sim/DrawPolicy.h"
#include "mugen/sim/buff/BuffRules.h"
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
// 逐帧观察器。
class Exporter
{
public:
    Exporter(World& world, summary::Summary& out) : m_world(world), m_out(out) {}

    void scanInitial()
    {
        m_world.ecs().each<Identity, RoleState, MotionState>(
            [&](Entity entity, const Identity& identity, const RoleState& state, const MotionState& motion) {
                if (identity.m_kind != SpawnKind::Role)
                    return;
                RoleWatch watch;
                watch.key    = roleKey(identity);
                watch.status = statusNames(role::statusBits(state.m_state));
                watch.action = motion.m_motionName;
                m_roles[entity.value] = watch;
            });
    }
    void afterTick(int64_t frame)
    {
        for (const std::string& line : m_world.traceLines())
            handleEvent(frame, line);
        observeRoles(frame);
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
    static std::string roleKey(const Identity& identity)
    {
        return "role:" + std::to_string(identity.m_configId) + "#" + std::to_string(identity.m_ordinal);
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
    }
    void observeRoles(int64_t frame)
    {
        for (auto& [value, watch] : m_roles)
        {
            const Entity entity{value};
            if (!m_world.ecs().alive(entity) || !m_world.ecs().has<RoleState>(entity))
                continue;
            const auto status = statusNames(role::statusBits(m_world.ecs().get<RoleState>(entity).m_state));
            if (status != watch.status)
            {
                emit(frame, "state", {watch.key, watch.status.empty() ? "-" : watch.status, status});
                watch.status = status;
            }
            if (const auto* motion = m_world.ecs().tryGet<MotionState>(entity))
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
            const auto& health = m_world.ecs().get<Health>(entity);
            const auto& pos    = m_world.ecs().get<Transform>(entity).m_position;
            watch.hp           = health.m_hp.toInt();
            watch.x            = toMilli(pos.x);
            watch.y            = toMilli(pos.y);
            watch.z            = toMilli(pos.z);
        }
    }

    World& m_world;
    summary::Summary& m_out;
    std::map<uint32_t, RoleWatch> m_roles;
};
}  // namespace

// 在该帧 tick 之前按脚本顺序准入 Buff。目标按当下创建顺序解析。
void applyBuffs(World& world, uint64_t frame, const std::vector<BuffInject>& buffs)
{
    Entity hero;
    std::vector<Entity> dummies;
    world.ecs().each<Identity>([&](Entity entity, const Identity& identity) {
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
