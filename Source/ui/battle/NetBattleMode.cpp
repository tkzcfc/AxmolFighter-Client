#include "ui/battle/NetBattleMode.h"

#include "axmol.h"
#include "mugen/render/battle/WorldPresenter.h"
#include "mugen/sim_legacy/BattleBootstrap.h"
#include "mugen/sim_legacy/net/NetCodec.h"
#include "mugen/core/serialize/ByteBuffer.h"
#include "AppContext.h"
#include "core/event/EventDispatcher.h"
#include "core/event/GameEvents.h"
#include "net/NetAgent.h"
#include "ui/progress/SessionFashionAppearance.h"
#include "ui/widgets/common/MessageDialog.h"
#include "ui/widgets/pet/PetLocalState.h"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace gameui
{
namespace
{
// 逻辑帧步长由服务器 frame_ms 决定，渲染插值 alpha 用墙钟帧内进度近似。
constexpr uint32_t kMaxCatchUpFrames = 4;
// 一秒窗口内最长渲染帧间隔超过该毫秒数时记录一条卡顿日志。
constexpr double kHitchLogThresholdMs = 60.0;

int64_t monotonicNowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// 高精度毫秒计时，仅用于调试统计。
double preciseNowMs()
{
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}
}  // namespace

class NetBattleMode::WorldPresenterHolder
{
public:
    explicit WorldPresenterHolder(ax::Node& root) : root(&root), presenter(std::in_place, root) {}
    ~WorldPresenterHolder()
    {
        // AvatarPresenter::clear 会访问场景节点，必须在摘掉 root 之前销毁 presenter。
        presenter.reset();
        if (root)
            root->removeFromParent();
    }

    ax::Node* root;
    std::optional<mugen::WorldPresenter> presenter;
};

NetBattleMode::NetBattleMode(NetBattleParams params, net::NetAgent& agent)
    : m_params(std::move(params)), m_agent(agent)
{
}

NetBattleMode::~NetBattleMode() = default;

bool NetBattleMode::init()
{
    registerPushes();
    sendReady();
    m_initialized = true;
    AXLOGI("NetBattleMode: ready sent (battle={})", m_params.battleId);
    return true;
}

void NetBattleMode::registerPushes()
{
    m_agent.listenPush([this](const PB::Battle::BattleStartPush& push) {
        if (push.battle_id() != m_params.battleId)
            return;
        onBattleStart(push);
    });
    m_agent.listenPush([this](const PB::Battle::BattleFramesPush& push) {
        if (push.battle_id() != m_params.battleId)
            return;
        deliverFrames(push);
    });
    m_agent.listenPush([this](const PB::Battle::BattleSnapshotPush& push) {
        if (push.battle_id() != m_params.battleId)
            return;
        deliverSnapshot(push);
    });
    m_agent.listenPush([this](const PB::Battle::BattleEndPush& push) {
        if (push.battle_id() != m_params.battleId)
            return;
        onEnd(push);
    });
}

void NetBattleMode::sendReady()
{
    PB::Battle::BattleReadyReq req;
    req.set_battle_id(m_params.battleId);
    m_agent.notify(req);
}

void NetBattleMode::onBattleStart(const PB::Battle::BattleStartPush& push)
{
    if (m_started)
        return;

    mugen::sim_legacy::WorldSetup setup;
    if (!mugen::sim_legacy::net::decodeWorldSetup(push.setup(), setup))
    {
        AXLOGE("NetBattleMode: failed to decode WorldSetup (battle={})", m_params.battleId);
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }

    m_localSlot = push.local_slot();
    m_frameMs   = push.frame_ms() > 0 ? push.frame_ms() : 33;

    mugen::sim_legacy::World world;
    const auto heroes = mugen::sim_legacy::bootstrapBattle(world, setup);
    if (heroes.empty())
    {
        AXLOGE("NetBattleMode: bootstrap battle failed (battle={})", m_params.battleId);
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }

    const auto& slots = world.ecs().singleton<mugen::sim_legacy::PlayerSlots>().entities;
    if (const auto it = slots.find(m_localSlot); it != slots.end())
        m_localPlayer = it->second;

    mugen::sim_legacy::net::NetBattleSyncConfig config;
    config.m_inputDelayFrames = push.input_delay_frames();
    config.m_frameMs          = m_frameMs;
    m_sync = std::make_unique<mugen::sim_legacy::net::NetBattleSync>(m_localSlot, push.slot_count(), config);
    m_sync->start(std::move(world), push.start_server_time_ms(), static_cast<uint64_t>(monotonicNowMs()));

    auto* scene = ax::Director::getInstance()->getRunningScene();
    auto* root  = ax::Node::create();
    scene->addChild(root, 10);
    m_presenter = std::make_unique<WorldPresenterHolder>(*root);
    m_presenter->presenter->loadMap(setup.mapKey);
    if (m_localPlayer.value != 0)
        m_presenter->presenter->setFollowEntity(m_localPlayer);
    m_presenter->presenter->setPet(resolveFollowPetId());
    m_presenter->presenter->refreshLocalPlayerAppearance(m_sync->session().world());
    applyRemoteAppearances(push);
    subscribeAppearanceEvents();

    // 战斗已在进行（断线重连 / 迟到入座）：世界此刻只是帧 0 的引导状态，紧随其后的快照才是当前帧。
    // 快照到达前不推进、不对账，避免从帧 0 重新模拟到当前帧。
    m_awaitSnapshot = push.resume();
    m_started       = true;
    AXLOGI("NetBattleMode: battle {} started (slot={}/{}, frame_ms={}, delay={}, resume={})",
           m_params.battleId,
           m_localSlot,
           push.slot_count(),
           m_frameMs,
           push.input_delay_frames(),
           push.resume());
}

void NetBattleMode::applyRemoteAppearances(const PB::Battle::BattleStartPush& push)
{
    const auto& slots = m_sync->session().world().ecs().singleton<mugen::sim_legacy::PlayerSlots>().entities;
    // 外观只影响表现层：按槽位找到对端实体，用服务端下发的外观覆盖职业默认外观。
    for (const auto& player : push.players())
    {
        if (player.slot() == m_localSlot || !player.has_appearance())
            continue;
        const auto it = slots.find(player.slot());
        if (it == slots.end())
            continue;
        m_presenter->presenter->setEntityAppearance(it->second, fashionAppearanceFromTown(player.appearance()));
        m_presenter->presenter->setEntityPet(it->second, player.appearance().pet_id());
    }
}

void NetBattleMode::onFrames(const PB::Battle::BattleFramesPush& push)
{
    if (!m_started || m_awaitSnapshot)
        return;

    const double netBegin        = preciseNowMs();
    const uint64_t resimBefore   = m_sync->session().stats().resimFrames;
    const uint64_t rollbacksBefore = m_sync->session().stats().rollbacks;
    // 回滚在同一帧号上重推：记下回滚前各角色位置，回滚后的差就是这次纠正量，交给表现层平滑消化而不是硬跳。
    std::vector<std::pair<mugen::Entity, mugen::FixedVec3>> rolesBefore;
    collectRolePositions(rolesBefore);

    // 先应用确认输入，再做哈希对账：确认输入触发的回滚会把预测重推到一致状态。
    for (const auto& frame : push.frames())
    {
        mugen::sim_legacy::FrameInput input;
        if (!mugen::sim_legacy::net::decodeFrameInput(frame.input(), input))
        {
            AXLOGW("NetBattleMode: decode confirmed frame {} failed", frame.frame());
            continue;
        }
        for (const auto& player : input.players)
            m_sync->confirmInput(input.frame, player.slot, player);
        for (const auto& command : input.systemCommands)
            m_sync->confirmSystem(input.frame, command);
    }

    const auto localNowMs = static_cast<uint64_t>(monotonicNowMs());
    m_sync->onServerClock(push.server_frame(), push.server_time_ms(), localNowMs);
    mugen::sim_legacy::net::ServerInputFeedback feedback;
    feedback.m_echoClientTimeMs = push.echo_client_time_ms();
    feedback.m_slackMin         = push.input_slack_min();
    feedback.m_slackFrame       = push.input_slack_frame();
    feedback.m_slackSamples     = push.input_slack_samples();
    feedback.m_lateInputs       = push.late_inputs();
    const uint32_t leadBefore   = m_sync->inputLeadFrames();
    const uint32_t delayBefore  = m_sync->localInputDelayFrames();
    m_sync->onServerFeedback(feedback, localNowMs);
    if (m_sync->inputLeadFrames() != leadBefore)
        AXLOGI("NetBattleMode: input lead {} -> {} (slack_min={} rtt={}ms late_inputs={})",
               leadBefore,
               m_sync->inputLeadFrames(),
               push.input_slack_min(),
               m_sync->rttMs(),
               m_sync->lateInputs());
    if (m_sync->localInputDelayFrames() != delayBefore)
        AXLOGI("NetBattleMode: local input delay {} -> {} (rtt={}ms lead={})",
               delayBefore,
               m_sync->localInputDelayFrames(),
               m_sync->rttMs(),
               m_sync->inputLeadFrames());

    // 刷新待回滚后再对账，避免用回滚前的陈旧快照误判失步。
    m_sync->flushRollback();
    if (m_sync->session().stats().rollbacks != rollbacksBefore)
        applyRollbackCorrections(rolesBefore);
    for (const auto& hash : push.hashes())
        if (m_sync->checkServerHash(hash.frame(), hash.hash()))
            sendResync(hash.frame());

    recordNetCallback(preciseNowMs() - netBegin, m_sync->session().stats().resimFrames - resimBefore);
}

void NetBattleMode::collectRolePositions(std::vector<std::pair<mugen::Entity, mugen::FixedVec3>>& out)
{
    m_sync->session().world().ecs().each<mugen::sim_legacy::EntityGroup, mugen::sim_legacy::Transform>(
        [&out](mugen::Entity entity, const mugen::sim_legacy::EntityGroup& group, const mugen::sim_legacy::Transform& transform) {
            if (group.kind == mugen::sim_legacy::GroupKind::Role)
                out.emplace_back(entity, transform.position);
        });
}

void NetBattleMode::applyRollbackCorrections(const std::vector<std::pair<mugen::Entity, mugen::FixedVec3>>& before)
{
    std::vector<std::pair<mugen::Entity, mugen::FixedVec3>> after;
    collectRolePositions(after);
    for (const auto& [entity, oldPos] : before)
    {
        for (const auto& [afterEntity, newPos] : after)
        {
            if (afterEntity != entity)
                continue;
            const mugen::VisualPos shift{(newPos.x - oldPos.x).toFloatForRender(), (newPos.y - oldPos.y).toFloatForRender(),
                                         (newPos.z - oldPos.z).toFloatForRender()};
            if (shift.x == 0.0f && shift.y == 0.0f && shift.z == 0.0f)
                break;
            m_presenter->presenter->applyCorrection(entity, shift);
            const float length = std::sqrt(shift.x * shift.x + shift.y * shift.y + shift.z * shift.z);
            if (entity == m_localPlayer)
            {
                ++m_localCorrections;
                m_lastLocalCorrection  = length;
                m_worstLocalCorrection = std::max(m_worstLocalCorrection, length);
            }
            else
            {
                ++m_remoteCorrections;
                m_lastRemoteCorrection  = length;
                m_worstRemoteCorrection = std::max(m_worstRemoteCorrection, length);
            }
            break;
        }
    }
}

int64_t NetBattleMode::simulatedDelayMs(int32_t baseMs)
{
    m_simRng = m_simRng * 1664525u + 1013904223u;
    const int64_t jitter = m_sim.m_jitterMs > 0 ? static_cast<int64_t>((m_simRng >> 8) % static_cast<uint32_t>(m_sim.m_jitterMs + 1)) : 0;
    return static_cast<int64_t>(baseMs) + jitter;
}

void NetBattleMode::deliverFrames(const PB::Battle::BattleFramesPush& push)
{
    if (m_sim.m_downMs <= 0 && m_sim.m_jitterMs <= 0 && m_inbound.empty())
    {
        onFrames(push);
        return;
    }
    m_lastInboundDueMs = std::max(m_lastInboundDueMs, monotonicNowMs() + simulatedDelayMs(m_sim.m_downMs));
    m_inbound.push_back({m_lastInboundDueMs, push});
}

void NetBattleMode::deliverSnapshot(const PB::Battle::BattleSnapshotPush& push)
{
    if (m_sim.m_downMs <= 0 && m_sim.m_jitterMs <= 0 && m_inbound.empty())
    {
        onSnapshot(push);
        return;
    }
    m_lastInboundDueMs = std::max(m_lastInboundDueMs, monotonicNowMs() + simulatedDelayMs(m_sim.m_downMs));
    m_inbound.push_back({m_lastInboundDueMs, push});
}

void NetBattleMode::dispatchInputPush(PB::Battle::BattleInputPush push)
{
    if (m_sim.m_upMs <= 0 && m_sim.m_jitterMs <= 0 && m_outbound.empty())
    {
        m_agent.notify(push);
        return;
    }
    m_lastOutboundDueMs = std::max(m_lastOutboundDueMs, monotonicNowMs() + simulatedDelayMs(m_sim.m_upMs));
    m_outbound.push_back({m_lastOutboundDueMs, std::move(push)});
}

void NetBattleMode::flushSimulatedMessages(int64_t nowMs)
{
    while (!m_inbound.empty() && m_inbound.front().m_dueMs <= nowMs)
    {
        auto delayed = std::move(m_inbound.front());
        m_inbound.pop_front();
        if (const auto* frames = std::get_if<PB::Battle::BattleFramesPush>(&delayed.m_push))
            onFrames(*frames);
        else
            onSnapshot(std::get<PB::Battle::BattleSnapshotPush>(delayed.m_push));
    }
    while (!m_outbound.empty() && m_outbound.front().m_dueMs <= nowMs)
    {
        m_agent.notify(m_outbound.front().m_push);
        m_outbound.pop_front();
    }
}

bool NetBattleMode::netInfo(BattleNetInfo& out) const
{
    if (!m_started)
        return false;
    out.m_rttMs      = m_sync->rttMs();
    out.m_jitterMs   = m_sync->rttJitterMs();
    out.m_leadFrames = m_sync->inputLeadFrames();
    out.m_frameMs    = m_frameMs;
    out.m_localDelayFrames = m_sync->localInputDelayFrames();
    switch (m_sync->localDelayMode())
    {
    case mugen::sim_legacy::net::LocalDelayMode::Auto:
        out.m_localDelayMode = "auto";
        break;
    case mugen::sim_legacy::net::LocalDelayMode::Forced:
        out.m_localDelayMode = "forced";
        break;
    case mugen::sim_legacy::net::LocalDelayMode::Off:
        out.m_localDelayMode = "off";
        break;
    }
    return true;
}

void NetBattleMode::recordNetCallback(double ms, uint64_t resimFrames)
{
    m_perfWindow.netMsTotal += ms;
    m_perfWindow.netMsMax = std::max(m_perfWindow.netMsMax, ms);
    m_frameResim += resimFrames;
}

void NetBattleMode::onSnapshot(const PB::Battle::BattleSnapshotPush& push)
{
    if (!m_started)
        return;

    mugen::ByteBuffer buffer;
    for (const char byte : push.world())
        buffer.writeByte(static_cast<uint8_t>(byte));
    buffer.writeFinish();
    if (!m_sync->resync(push.frame(), buffer))
    {
        AXLOGW("NetBattleMode: resync to frame {} failed", push.frame());
        return;
    }
    // 快照携带服务器当前时间：直接重置时钟，使目标帧立即对齐到快照帧附近
    // （重连时快照帧远大于 0，不能靠平滑偏移慢慢收敛）。
    m_sync->resetClock(push.server_time_ms(), static_cast<uint64_t>(monotonicNowMs()));
    m_awaitSnapshot = false;
    AXLOGI("NetBattleMode: resynced to frame {}", push.frame());
}

void NetBattleMode::onEnd(const PB::Battle::BattleEndPush& push)
{
    if (m_ended)
        return;
    m_ended = true;

    bool win = false;
    for (const auto& result : push.players())
        if (result.slot() == m_localSlot)
            win = result.win();
    const bool draw = push.winner_camp() == 0;
    AXLOGI("NetBattleMode: battle {} ended winner_camp={} reason={} end_frame={}",
           m_params.battleId,
           push.winner_camp(),
           push.reason(),
           push.end_frame());
    showResult(win, draw);
}

void NetBattleMode::showResult(bool win, bool draw)
{
    const std::string text = draw ? "平局" : (win ? "胜利" : "失败");
    MessageDialog::show(text, [this]() { m_returnToTown = true; });
}

void NetBattleMode::sendInputPush()
{
    PB::Battle::BattleInputPush push;
    push.set_battle_id(m_params.battleId);
    push.set_ack_frame(static_cast<uint32_t>(m_sync->session().confirmedFrame()));
    push.set_client_time_ms(static_cast<uint64_t>(monotonicNowMs()));

    std::vector<std::pair<uint64_t, mugen::sim_legacy::PlayerInput>> inputs;
    m_sync->collectPendingInputs(inputs);
    for (auto& [frame, input] : inputs)
    {
        auto* entry = push.add_inputs();
        entry->set_frame(static_cast<uint32_t>(frame));
        entry->set_input(mugen::sim_legacy::net::encodePlayerInput(input));
    }

    std::vector<std::pair<uint64_t, uint64_t>> hashes;
    m_sync->collectClientHashes(hashes);
    for (const auto& [frame, hash] : hashes)
    {
        auto* entry = push.add_client_hashes();
        entry->set_frame(static_cast<uint32_t>(frame));
        entry->set_hash(hash);
    }

    dispatchInputPush(std::move(push));
}

void NetBattleMode::sendResync(uint32_t frame)
{
    PB::Battle::BattleResyncReq req;
    req.set_battle_id(m_params.battleId);
    req.set_frame(frame);
    m_agent.notify(req);
    ++m_resyncRequests;
    AXLOGW("NetBattleMode: hash mismatch at frame {}, requesting resync", frame);
}

void NetBattleMode::requestLeave()
{
    if (m_leaveSent || m_ended)
        return;
    m_leaveSent = true;
    PB::Battle::BattleLeaveReq req;
    req.set_battle_id(m_params.battleId);
    m_agent.notify(req);
    AXLOGI("NetBattleMode: leave requested (battle={})", m_params.battleId);
}

void NetBattleMode::onUpdate(float delta)
{
    flushSimulatedMessages(monotonicNowMs());
    if (!m_started || m_ended || m_awaitSnapshot)
        return;

    const double simBegin = preciseNowMs();
    const auto nowMs      = static_cast<uint64_t>(monotonicNowMs());
    const uint64_t target = m_sync->targetFrame(nowMs);
    uint32_t catchUp      = 0;
    bool advanced         = false;
    // 每个帧号只生成并提交一次输入：先确认预测窗口还允许推进，再消费控制命令，
    // 避免被停顿的帧把一次性的边缘命令吞掉。输入领先量缩小时 target 低于当前帧，此处自然等待。
    // 采样结果由 NetBattleSync 安排到「本帧 + 本地输入延迟 D」帧生效并提前发出，本帧自身用此前安排的输入。
    while (m_sync->session().currentFrame() < target && catchUp < kMaxCatchUpFrames && m_sync->session().canAdvance())
    {
        const uint64_t frame = m_sync->session().currentFrame() + 1;
        std::vector<mugen::sim_legacy::PlayerCommand> commands;
        // 开局校准与领先量增大后的追帧期间，这些帧的输入送到服务器时早已过期；此时不采样控制，
        // 按键状态保留，追上之后的第一帧照常发出，避免角色起步即被服务器改写、回滚拉回。
        // D 刚减小时本帧的生效帧已被占用，同样跳过一次采样（按键状态保留到下一帧）。
        if (m_sync->inputReady(nowMs, frame))
            m_control.advance(commands);
        mugen::sim_legacy::PlayerInput input;
        input.slot      = m_localSlot;
        input.commands  = std::move(commands);
        m_sync->submitLocalInput(frame, std::move(input));
        if (!m_sync->session().tick())
            break;  // 仅在待回滚无法执行时发生，预测窗口耗尽已由 canAdvance 提前挡住。
        advanced = true;
        ++catchUp;
    }
    if (advanced)
        sendInputPush();
    {
        const double simMs = preciseNowMs() - simBegin;
        m_perfWindow.simMsTotal += simMs;
        m_perfWindow.simMsMax = std::max(m_perfWindow.simMsMax, simMs);
        m_perfWindow.tickedFrames += catchUp;
        if (catchUp > 1)
            m_perfWindow.catchUpFrames += catchUp - 1;
        m_perfWindow.maxTicksInFrame = std::max(m_perfWindow.maxTicksInFrame, catchUp);
    }

    // 渲染插值进度：从本帧逻辑 tick 起算（与 tick 同相），tick 迟到时停在 1，不回绕。
    const float step = static_cast<float>(m_frameMs) / 1000.0f;
    m_tickAlpha.onRenderFrame(delta, advanced);

    auto& session = m_sync->session();
    for (const auto& event : session.eventsToPresent())
        if (std::holds_alternative<mugen::sim_legacy::HurtNumber>(event.payload))
        {
            const auto* identity = session.world().ecs().tryGet<mugen::sim_legacy::RoleIdentity>(event.entity);
            if (identity && identity->roleType != static_cast<int32_t>(mugen::EntityRoleType::kHero))
                m_lastHurtTarget = event.entity;
        }
    const double presentBegin = preciseNowMs();
    m_presenter->presenter->presentEvents(session.eventsToPresent(), session.world(), m_tickAlpha.alpha(step), delta);
    session.clearEventsToPresent();
    const double presentMs = preciseNowMs() - presentBegin;
    m_perfWindow.presentMsTotal += presentMs;
    m_perfWindow.presentMsMax = std::max(m_perfWindow.presentMsMax, presentMs);
    finishRenderFrame(delta);
}

void NetBattleMode::finishRenderFrame(float delta)
{
    ++m_perfWindow.renderFrames;
    m_perfWindow.deltaMsMax = std::max(m_perfWindow.deltaMsMax, static_cast<double>(delta) * 1000.0);
    m_perfWindow.maxResimInFrame = std::max(m_perfWindow.maxResimInFrame, static_cast<uint32_t>(m_frameResim));
    m_frameResim = 0;

    m_perfWindowElapsed += delta;
    if (m_perfWindowElapsed < 1.0f)
        return;
    // 一秒窗口结束：回滚计数取会话累计统计的差值，其余由逐帧累计。
    const auto& stats = m_sync->session().stats();
    m_perfWindow.rollbacks           = stats.rollbacks - m_perfStatsBase.rollbacks;
    m_perfWindow.resimFrames         = stats.resimFrames - m_perfStatsBase.resimFrames;
    m_perfWindow.mispredictions      = stats.mispredictions - m_perfStatsBase.mispredictions;
    m_perfWindow.localMispredictions = stats.localMispredictions - m_perfStatsBase.localMispredictions;
    m_perfWindow.stalls              = stats.stalls - m_perfStatsBase.stalls;
    m_perfStatsBase                  = stats;
    // 一秒内出现过明显卡顿才记一行日志（最多每秒一条），附各阶段最大耗时便于定位。
    if (m_perfWindow.deltaMsMax > kHitchLogThresholdMs)
        AXLOGW("NetBattleMode: slow frame in last 1s: worst={:.1f}ms sim_max={:.2f}ms net_max={:.2f}ms "
               "present_max={:.2f}ms resim_max={} rollbacks={}",
               m_perfWindow.deltaMsMax,
               m_perfWindow.simMsMax,
               m_perfWindow.netMsMax,
               m_perfWindow.presentMsMax,
               m_perfWindow.maxResimInFrame,
               m_perfWindow.rollbacks);
    m_perfLast                       = m_perfWindow;
    m_perfWorst.deltaMsMax           = std::max(m_perfWorst.deltaMsMax, m_perfWindow.deltaMsMax);
    m_perfWorst.maxResimInFrame      = std::max(m_perfWorst.maxResimInFrame, m_perfWindow.maxResimInFrame);
    m_perfWorst.maxTicksInFrame      = std::max(m_perfWorst.maxTicksInFrame, m_perfWindow.maxTicksInFrame);
    m_perfWorst.simMsMax             = std::max(m_perfWorst.simMsMax, m_perfWindow.simMsMax);
    m_perfWorst.netMsMax             = std::max(m_perfWorst.netMsMax, m_perfWindow.netMsMax);
    m_perfWorst.presentMsMax         = std::max(m_perfWorst.presentMsMax, m_perfWindow.presentMsMax);
    m_perfWindow                     = {};
    m_perfWindowElapsed              = 0.0f;
}

void NetBattleMode::setInput(uint32_t slot, bool pressed)
{
    if (slot < static_cast<uint32_t>(mugen::sim_legacy::ControlKey::Count))
        m_control.setKey(static_cast<mugen::sim_legacy::ControlKey>(slot), pressed);
}

void NetBattleMode::onExit()
{
    // 离开界面（关闭客户端 / 断线 / 切屏）不是投降：战斗在服务器继续，重新登录后可重连。
    // 只有玩家显式点击投降才发送 BattleLeaveReq（requestLeave）。
    if (auto* ed = AppContext::get().eventDispatcher())
        ed->unsubscribeAll(this);
    // 世界与表现层由析构函数释放（m_presenter 摘除根节点、m_sync 释放世界），
    // 保持 world() 在 HUD 关闭前始终有效。
    m_initialized = false;
    m_started     = false;
}

void NetBattleMode::subscribeAppearanceEvents()
{
    auto* ed = AppContext::get().eventDispatcher();
    if (!ed)
        return;
    auto refresh = [this]() { refreshLocalAppearance(); };
    ed->subscribe(game::GameEvent::FashionWornChanged, this, refresh);
    ed->subscribe(game::GameEvent::ArmamentAppearanceChanged, this, refresh);
}

void NetBattleMode::refreshLocalAppearance()
{
    if (!m_started)
        return;
    m_presenter->presenter->refreshLocalPlayerAppearance(m_sync->session().world());
}

int32_t NetBattleMode::resolveFollowPetId() const
{
    const int32_t activeId = PetLocalState::instance().activeId();
    return activeId > 0 ? activeId : 17102;
}

const mugen::sim_legacy::World& NetBattleMode::world() const
{
    return m_sync->session().world();
}

void NetBattleMode::renderDebugPanel()
{
    if (!m_started)
        return;

    const auto& session = m_sync->session();
    const auto& stats   = session.stats();
    const uint64_t current   = session.currentFrame();
    const uint64_t confirmed = session.confirmedFrame();

    ImGui::SetNextWindowPos(ImVec2(20.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("联网战斗");
    // 最醒目的一行：到战斗服务器的延迟，再附输入领先量。
    BattleNetInfo net;
    netInfo(net);
    BattleDebugPanel::renderNetLatency(net);
    ImGui::Separator();
    ImGui::Text("battle=%u slot=%u", m_params.battleId, m_localSlot);
    ImGui::Text("frame=%llu confirmed=%llu ahead=%llu",
                static_cast<unsigned long long>(current),
                static_cast<unsigned long long>(confirmed),
                static_cast<unsigned long long>(m_sync->aheadFrames()));
    if (m_sync->lateInputs() > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                           "late inputs=%u (last slack=%d)",
                           m_sync->lateInputs(),
                           m_sync->lastSlackFrames());
    else
        ImGui::Text("late inputs=0 (last slack=%d)", m_sync->lastSlackFrames());
    ImGui::Text("local hero corrections=%u last=%.1f worst=%.1f",
                m_localCorrections,
                m_lastLocalCorrection,
                m_worstLocalCorrection);
    ImGui::Text("remote hero corrections=%u last=%.1f worst=%.1f (smoothed)",
                m_remoteCorrections,
                m_lastRemoteCorrection,
                m_worstRemoteCorrection);
    ImGui::Text("rollbacks=%llu stalls=%llu mispredict=%llu (local=%llu remote=%llu) resync=%u",
                static_cast<unsigned long long>(stats.rollbacks),
                static_cast<unsigned long long>(stats.stalls),
                static_cast<unsigned long long>(stats.mispredictions),
                static_cast<unsigned long long>(stats.localMispredictions),
                static_cast<unsigned long long>(stats.remoteMispredictions),
                m_resyncRequests);
    ImGui::Text("clock_offset=%lldms", static_cast<long long>(m_sync->clockOffsetMs()));

    // 最近一个完整一秒窗口：用来判断卡顿来自回滚 / 模拟还是表现层。
    const auto& win = m_perfLast;
    const double frames = win.renderFrames > 0 ? static_cast<double>(win.renderFrames) : 1.0;
    ImGui::Separator();
    ImGui::Text("last 1s: render=%u fps, worst frame=%.1fms", win.renderFrames, win.deltaMsMax);
    ImGui::Text("rollbacks=%llu resim=%llu (max %u in one frame) mispredict=%llu",
                static_cast<unsigned long long>(win.rollbacks),
                static_cast<unsigned long long>(win.resimFrames),
                win.maxResimInFrame,
                static_cast<unsigned long long>(win.mispredictions));
    if (win.localMispredictions > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                           "local input rewritten by server x%llu (late input?)",
                           static_cast<unsigned long long>(win.localMispredictions));
    ImGui::Text("ticks=%u catch-up=%u (max %u/frame) stalls=%llu",
                win.tickedFrames,
                win.catchUpFrames,
                win.maxTicksInFrame,
                static_cast<unsigned long long>(win.stalls));
    ImGui::Text("sim    avg=%.3fms max=%.3fms", win.simMsTotal / frames, win.simMsMax);
    ImGui::Text("net    sum=%.3fms max=%.3fms (confirm + rollback)", win.netMsTotal, win.netMsMax);
    ImGui::Text("present avg=%.3fms max=%.3fms", win.presentMsTotal / frames, win.presentMsMax);
    ImGui::Separator();
    ImGui::Text("worst: frame=%.1fms resim/frame=%u ticks/frame=%u", m_perfWorst.deltaMsMax, m_perfWorst.maxResimInFrame, m_perfWorst.maxTicksInFrame);
    ImGui::Text("worst: sim=%.3fms net=%.3fms present=%.3fms", m_perfWorst.simMsMax, m_perfWorst.netMsMax, m_perfWorst.presentMsMax);
    ImGui::Separator();
    // 调试：本地输入延迟 D（输入晚 D 帧在本地生效，换取客户端少领先 D 帧、对端被回滚拉回的距离变小）。
    ImGui::TextUnformatted("本地输入延迟(调试)");
    bool delayChanged = ImGui::RadioButton("自动", &m_delayModeUi, 0);
    ImGui::SameLine();
    delayChanged |= ImGui::RadioButton("固定", &m_delayModeUi, 1);
    ImGui::SameLine();
    delayChanged |= ImGui::RadioButton("关闭", &m_delayModeUi, 2);
    delayChanged |= ImGui::SliderInt("固定帧数", &m_delayForcedUi, 0, 10);
    if (delayChanged)
    {
        constexpr mugen::sim_legacy::net::LocalDelayMode kModes[] = {mugen::sim_legacy::net::LocalDelayMode::Auto,
                                                              mugen::sim_legacy::net::LocalDelayMode::Forced,
                                                              mugen::sim_legacy::net::LocalDelayMode::Off};
        m_sync->setLocalDelayMode(kModes[m_delayModeUi], static_cast<uint32_t>(m_delayForcedUi));
    }
    ImGui::Separator();
    // 调试：本机复现高延迟。额外延迟只作用于本客户端收发的战斗消息，面板上的 RTT 会包含它。
    ImGui::TextUnformatted("模拟延迟(调试，0=关闭)");
    ImGui::SliderInt("上行额外延迟 ms", &m_sim.m_upMs, 0, 500);
    ImGui::SliderInt("下行额外延迟 ms", &m_sim.m_downMs, 0, 500);
    ImGui::SliderInt("抖动 ms", &m_sim.m_jitterMs, 0, 200);
    ImGui::Separator();
    if (ImGui::Button("投降", ImVec2(-1, 0)))
        requestLeave();
    ImGui::End();
    m_debugPanel.renderNet(m_sync->session().world(), net);
}

}  // namespace gameui
