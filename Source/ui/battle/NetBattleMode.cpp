#include "ui/battle/NetBattleMode.h"

#include "axmol.h"
#include "mugen/render/battle/WorldPresenter.h"
#include "mugen/sim/BattleBootstrap.h"
#include "mugen/sim/net/NetCodec.h"
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
#include <utility>

namespace gameui
{
namespace
{
// 逻辑帧步长由服务器 frame_ms 决定，渲染插值 alpha 用墙钟帧内进度近似。
constexpr uint32_t kMaxCatchUpFrames = 4;

int64_t monotonicNowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
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
        onFrames(push);
    });
    m_agent.listenPush([this](const PB::Battle::BattleSnapshotPush& push) {
        if (push.battle_id() != m_params.battleId)
            return;
        onSnapshot(push);
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

    mugen::sim::WorldSetup setup;
    if (!mugen::sim::net::decodeWorldSetup(push.setup(), setup))
    {
        AXLOGE("NetBattleMode: failed to decode WorldSetup (battle={})", m_params.battleId);
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }

    m_localSlot = push.local_slot();
    m_frameMs   = push.frame_ms() > 0 ? push.frame_ms() : 33;

    mugen::sim::World world;
    const auto heroes = mugen::sim::bootstrapBattle(world, setup);
    if (heroes.empty())
    {
        AXLOGE("NetBattleMode: bootstrap battle failed (battle={})", m_params.battleId);
        MessageDialog::showGlobal("初始化失败", []() { ax::Director::getInstance()->end(); });
        return;
    }

    const auto& slots = world.ecs().singleton<mugen::sim::PlayerSlots>().entities;
    if (const auto it = slots.find(m_localSlot); it != slots.end())
        m_localPlayer = it->second;

    mugen::sim::net::NetBattleSyncConfig config;
    config.m_inputDelayFrames = push.input_delay_frames();
    config.m_frameMs          = m_frameMs;
    m_sync = std::make_unique<mugen::sim::net::NetBattleSync>(m_localSlot, push.slot_count(), config);
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
    const auto& slots = m_sync->session().world().ecs().singleton<mugen::sim::PlayerSlots>().entities;
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

    // 先应用确认输入，再做哈希对账：确认输入触发的回滚会把预测重推到一致状态。
    for (const auto& frame : push.frames())
    {
        mugen::sim::FrameInput input;
        if (!mugen::sim::net::decodeFrameInput(frame.input(), input))
        {
            AXLOGW("NetBattleMode: decode confirmed frame {} failed", frame.frame());
            continue;
        }
        for (const auto& player : input.players)
            m_sync->confirmInput(input.frame, player.slot, player);
        for (const auto& command : input.systemCommands)
            m_sync->confirmSystem(input.frame, command);
    }

    m_sync->onServerClock(push.server_frame(), push.server_time_ms(), static_cast<uint64_t>(monotonicNowMs()));

    // 刷新待回滚后再对账，避免用回滚前的陈旧快照误判失步。
    m_sync->flushRollback();
    for (const auto& hash : push.hashes())
        if (m_sync->checkServerHash(hash.frame(), hash.hash()))
            sendResync(hash.frame());
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

    std::vector<std::pair<uint64_t, mugen::sim::PlayerInput>> inputs;
    m_sync->collectPendingInputs(inputs);
    for (auto& [frame, input] : inputs)
    {
        auto* entry = push.add_inputs();
        entry->set_frame(static_cast<uint32_t>(frame));
        entry->set_input(mugen::sim::net::encodePlayerInput(input));
    }

    std::vector<std::pair<uint64_t, uint64_t>> hashes;
    m_sync->collectClientHashes(hashes);
    for (const auto& [frame, hash] : hashes)
    {
        auto* entry = push.add_client_hashes();
        entry->set_frame(static_cast<uint32_t>(frame));
        entry->set_hash(hash);
    }

    m_agent.notify(push);
}

void NetBattleMode::sendResync(uint32_t frame)
{
    PB::Battle::BattleResyncReq req;
    req.set_battle_id(m_params.battleId);
    req.set_frame(frame);
    m_agent.notify(req);
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
    if (!m_started || m_ended || m_awaitSnapshot)
        return;

    const uint64_t target = m_sync->targetFrame(static_cast<uint64_t>(monotonicNowMs()));
    uint32_t catchUp      = 0;
    bool advanced         = false;
    while (m_sync->session().currentFrame() < target && catchUp < kMaxCatchUpFrames)
    {
        std::vector<mugen::sim::PlayerCommand> commands;
        m_control.advance(commands);
        mugen::sim::PlayerInput input;
        input.slot      = m_localSlot;
        input.commands  = std::move(commands);
        m_sync->submitLocalInput(m_sync->session().currentFrame() + 1, std::move(input));
        if (!m_sync->session().tick())
            break;  // 预测领先超出回滚窗口，优雅停顿。
        advanced = true;
        ++catchUp;
    }
    if (advanced)
        sendInputPush();

    // 渲染插值 alpha：按服务器帧长累计墙钟帧内进度。
    const float step = static_cast<float>(m_frameMs) / 1000.0f;
    m_renderAccum += delta;
    if (m_renderAccum >= step)
        m_renderAccum -= step;

    auto& session = m_sync->session();
    for (const auto& event : session.eventsToPresent())
        if (std::holds_alternative<mugen::sim::HurtNumber>(event.payload))
        {
            const auto* identity = session.world().ecs().tryGet<mugen::sim::RoleIdentity>(event.entity);
            if (identity && identity->roleType != static_cast<int32_t>(mugen::EntityRoleType::kHero))
                m_lastHurtTarget = event.entity;
        }
    m_presenter->presenter->presentEvents(session.eventsToPresent(), session.world(), m_renderAccum / step, delta);
    session.clearEventsToPresent();
}

void NetBattleMode::setInput(uint32_t slot, bool pressed)
{
    if (slot < static_cast<uint32_t>(mugen::sim::ControlKey::Count))
        m_control.setKey(static_cast<mugen::sim::ControlKey>(slot), pressed);
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

const mugen::sim::World& NetBattleMode::world() const
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
    ImGui::SetNextWindowSize(ImVec2(260.0f, 220.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("联网战斗");
    ImGui::Text("battle=%u slot=%u", m_params.battleId, m_localSlot);
    ImGui::Text("frame=%llu confirmed=%llu lead=%llu",
                static_cast<unsigned long long>(current),
                static_cast<unsigned long long>(confirmed),
                static_cast<unsigned long long>(m_sync->leadFrames()));
    ImGui::Text("rollbacks=%llu stalls=%llu mispredict=%llu",
                static_cast<unsigned long long>(stats.rollbacks),
                static_cast<unsigned long long>(stats.stalls),
                static_cast<unsigned long long>(stats.mispredictions));
    ImGui::Text("ping(est)=%lldms clock_offset=%lldms",
                static_cast<long long>(m_sync->estimatedOneWayDelayMs()),
                static_cast<long long>(m_sync->clockOffsetMs()));
    ImGui::Separator();
    if (ImGui::Button("投降", ImVec2(-1, 0)))
        requestLeave();
    ImGui::End();
}

}  // namespace gameui
