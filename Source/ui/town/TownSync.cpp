#include "ui/town/TownSync.h"

#include "mugen/conf/TypeConversions.h"
#include "mugen/sim/spawn/RoleSpawn.h"
#include "ui/progress/SessionFashionAppearance.h"
#include "ui/town/TownFixed.h"

#include <algorithm>
#include <chrono>

namespace gameui
{

namespace
{
// 服务端广播 tick（默认 10 Hz），与 game/town/town.toml [sync].tick_ms 对应。
constexpr float kTickMs = 100.0f;
// 渲染远端玩家时回放的插值延迟：约 2.5 × tick，留出足够缓冲平滑网络抖动。
constexpr float kInterpDelayMs = 2.5f * kTickMs;
// 缓冲不足时的外推上限：1 个 tick。
constexpr float kExtrapolationMs = kTickMs;
// 两个快照时间间隔超过该值视为状态缺口，直接吸附而非跨缺口插值。
constexpr float kSnapGapMs = 3.0f * kTickMs;
// 每个玩家保留的快照数量上限。
constexpr size_t kMaxSnapshots = 32;
// 时钟偏移的 EMA 平滑系数。
constexpr double kClockSmoothing = 0.1;

int64_t monotonicNowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

mugen::FixedVec3 movementPos(const PB::Types::TownMovement& movement)
{
    if (!movement.has_pos())
        return {};
    return {townFromFixedPoint(movement.pos().x()),
            townFromFixedPoint(movement.pos().y()),
            townFromFixedPoint(movement.pos().z())};
}
}  // namespace

TownSync::TownSync(mugen::sim::World& world, mugen::WorldPresenter& presenter, net::NetAgent& agent)
    : m_world(world)
    , m_presenter(presenter)
    , m_agent(agent)
{
}

void TownSync::start()
{
    m_agent.listenPush([this](PB::Town::ScenePlayerEnterPush& push) {
        if (push.has_player())
            addRemotePlayer(push.player());
    });

    m_agent.listenPush([this](PB::Town::ScenePlayerLeavePush& push) { removeRemotePlayer(push.player_id()); });

    m_agent.listenPush([this](PB::Town::ScenePlayerAppearancePush& push) {
        const auto it = m_players.find(push.player_id());
        if (it == m_players.end())
            return;
        if (push.has_appearance())
            applyAppearance(it->second.entity, push.appearance());
    });

    m_agent.listenPush([this](PB::Town::ScenePlayerStatesPush& push) {
        for (const auto& entry : push.players())
        {
            if (entry.player_id() == m_localPlayerId || !entry.has_movement())
                continue;
            updateClockOffset(entry.server_time_ms());
            pushSnapshot(entry.player_id(), entry.server_time_ms(), entry.movement());
        }
    });
}

void TownSync::applyEnterPlayers(const PB::Game::EnterSceneResp& resp)
{
    for (const auto& player : resp.players())
        addRemotePlayer(player);
}

void TownSync::tick(float /*logicDt*/)
{
    const uint64_t now   = estimatedServerNowMs();
    const uint64_t delay = static_cast<uint64_t>(kInterpDelayMs);
    const uint64_t renderTime = now > delay ? now - delay : 0;

    for (auto& entry : m_players)
    {
        Snapshot out;
        if (!sampleRemote(entry.second, renderTime, &out))
            continue;
        m_pendingPuppetUpdates.push_back(
            {entry.second.entity,
             toFixedVec3(out.x, out.y, out.z),
             out.towardX,
             out.moving ? mugen::EntityRoleStatus::Walk : mugen::EntityRoleStatus::Idle,
             out.motionName});
    }
}

std::vector<mugen::sim::PuppetUpdate> TownSync::takePuppetUpdates()
{
    std::vector<mugen::sim::PuppetUpdate> result;
    result.swap(m_pendingPuppetUpdates);
    return result;
}

void TownSync::clear()
{
    for (auto& entry : m_players)
    {
        m_presenter.clearEntityPet(entry.second.entity);
        m_world.destroy(entry.second.entity);
    }
    m_players.clear();
    m_pendingPuppetUpdates.clear();
}

void TownSync::addRemotePlayer(const PB::Types::TownPlayerInfo& info)
{
    if (info.player_id() == m_localPlayerId)
        return;

    const auto existing = m_players.find(info.player_id());
    if (existing != m_players.end())
    {
        if (info.has_appearance())
            applyAppearance(existing->second.entity, info.appearance());
        if (info.has_movement())
            pushSnapshot(info.player_id(), estimatedServerNowMs(), info.movement());
        return;
    }

    const auto& appearance = info.appearance();
    const int32_t roleId =
        mugen::type_conversions::toRoleConfigId(static_cast<mugen::CharacterClass>(appearance.class_id()));
    const int32_t toward = info.movement().facing() == 0 ? -1 : 1;

    const mugen::Entity entity =
        mugen::sim::RoleSpawner(m_world).spawnPuppet(info.player_id(), roleId, movementPos(info.movement()), toward);
    if (entity.value == 0)
        return;

    RemotePlayer player;
    player.entity = entity;
    m_players.emplace(info.player_id(), std::move(player));

    applyAppearance(entity, appearance);
    // 进入快照没有 server_time_ms，用「估计服务端现在」作为基准时间。
    pushSnapshot(info.player_id(), estimatedServerNowMs(), info.movement());
}

void TownSync::removeRemotePlayer(int64_t playerId)
{
    const auto it = m_players.find(playerId);
    if (it == m_players.end())
        return;
    m_presenter.clearEntityPet(it->second.entity);
    m_world.destroy(it->second.entity);
    m_players.erase(it);
}

void TownSync::applyAppearance(mugen::Entity entity, const PB::Types::TownAppearance& appearance)
{
    mugen::FashionAppearance fa;
    const auto cls = static_cast<mugen::CharacterClass>(appearance.class_id());
    fa.roleId      = mugen::type_conversions::toRoleConfigId(cls);
    for (int i = 0; i < appearance.base_skins_size(); ++i)
    {
        const auto& skin = appearance.base_skins(i);
        if (skin.res_fashion_id() <= 0)
            continue;
        fa.baseFashion[static_cast<mugen::FashionPosition>(skin.position())] = skin.res_fashion_id();
    }
    for (int i = 0; i < appearance.fashions_size(); ++i)
    {
        const auto& item = appearance.fashions(i);
        if (item.config_id() <= 0)
            continue;
        fa.equipFashion[static_cast<mugen::FashionPosition>(item.position())] = item.config_id();
    }
    const int32_t weapon = armamentWeaponSpineId(appearance.armament_id(), cls);
    if (weapon > 0)
        fa.baseFashion[mugen::FashionPosition::kWeapon] = weapon;
    m_presenter.setEntityAppearance(entity, fa);
    m_presenter.setEntityPet(entity, appearance.pet_id());
}

void TownSync::pushSnapshot(int64_t playerId, uint64_t serverTimeMs, const PB::Types::TownMovement& movement)
{
    const auto it = m_players.find(playerId);
    if (it == m_players.end())
        return;

    auto& snapshots = it->second.snapshots;
    if (!snapshots.empty() && serverTimeMs <= snapshots.rbegin()->first)
        return;  // 过期/重复快照忽略

    Snapshot snapshot;
    snapshot.serverTimeMs = serverTimeMs;
    if (movement.has_pos())
    {
        snapshot.x = movement.pos().x() / 1000.0f;
        snapshot.y = movement.pos().y() / 1000.0f;
        snapshot.z = movement.pos().z() / 1000.0f;
    }
    if (movement.has_vel())
    {
        snapshot.vx = movement.vel().x();
        snapshot.vy = movement.vel().y();
        snapshot.vz = movement.vel().z();
    }
    snapshot.towardX    = movement.facing() == 0 ? -1 : 1;
    snapshot.moving     = movement.moving();
    snapshot.motionName = movement.state();

    snapshots.emplace(serverTimeMs, std::move(snapshot));
    while (snapshots.size() > kMaxSnapshots)
        snapshots.erase(snapshots.begin());
}

void TownSync::updateClockOffset(uint64_t serverTimeMs)
{
    const double sample = static_cast<double>(serverTimeMs) - static_cast<double>(monotonicNowMs());
    if (!m_hasOffset)
    {
        m_clockOffsetMs = sample;
        m_hasOffset     = true;
    }
    else
    {
        m_clockOffsetMs += (sample - m_clockOffsetMs) * kClockSmoothing;
    }
}

uint64_t TownSync::estimatedServerNowMs() const
{
    const int64_t now  = monotonicNowMs();
    const int64_t est  = now + static_cast<int64_t>(m_clockOffsetMs);
    return est > 0 ? static_cast<uint64_t>(est) : 0;
}

bool TownSync::sampleRemote(const RemotePlayer& player, uint64_t renderTimeMs, Snapshot* out) const
{
    if (player.snapshots.empty())
        return false;

    const auto first = player.snapshots.begin();
    const auto last  = std::prev(player.snapshots.end());

    if (renderTimeMs <= first->first)
    {
        *out = first->second;
        return true;
    }
    if (renderTimeMs >= last->first)
    {
        // 缓冲不足：沿最近速度外推，最多 1 个 tick。
        const double dtMs = std::min<double>(renderTimeMs - last->first, kExtrapolationMs);
        const double dtSec = dtMs / 1000.0;
        Snapshot s         = last->second;
        s.x += static_cast<float>(s.vx * dtSec);
        s.y += static_cast<float>(s.vy * dtSec);
        s.z += static_cast<float>(s.vz * dtSec);
        *out = s;
        return true;
    }

    const auto upper = player.snapshots.upper_bound(renderTimeMs);
    if (upper == player.snapshots.end() || upper == player.snapshots.begin())
    {
        *out = (upper == player.snapshots.end() ? last->second : upper->second);
        return true;
    }

    const auto lower = std::prev(upper);
    const uint64_t gap = upper->first - lower->first;
    if (gap > static_cast<uint64_t>(kSnapGapMs))
    {
        // 状态缺口：直接吸附到新快照，避免跨缺口插值拖尾。
        *out = upper->second;
        return true;
    }

    const float alpha = static_cast<float>(static_cast<double>(renderTimeMs - lower->first) / static_cast<double>(gap));
    Snapshot s;
    s.x = lower->second.x + (upper->second.x - lower->second.x) * alpha;
    s.y = lower->second.y + (upper->second.y - lower->second.y) * alpha;
    s.z = lower->second.z + (upper->second.z - lower->second.z) * alpha;
    s.towardX    = upper->second.towardX;
    s.moving     = upper->second.moving;
    s.motionName = upper->second.motionName;
    *out         = s;
    return true;
}

mugen::FixedVec3 TownSync::toFixedVec3(float x, float y, float z)
{
    return {townPxToFixed(x), townPxToFixed(y), townPxToFixed(z)};
}

}  // namespace gameui
