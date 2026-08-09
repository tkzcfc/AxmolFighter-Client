#include "mugen/net/BattleNetSync.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/core/serialize/ByteBuffer.h"

using namespace mugen;

namespace gameui
{

void BattleNetSync::start(mugen::GameWord* world,
                          std::uint32_t battleId,
                          std::uint32_t serverFrame,
                          std::uint32_t localActorId)
{
    m_world           = world;
    m_battleId        = battleId;
    m_localActorId    = localActorId;
    m_lastServerFrame = serverFrame;
    m_clientFrame     = 0;
    m_accumulator     = 0.0f;
    m_inputHistory.clear();
    m_pendingInputs.clear();
    m_running = true;

    if (m_world)
        m_world->bindLocalPlayer(static_cast<mugen::EntityId>(m_localActorId));
}

void BattleNetSync::stop()
{
    m_running = false;
    m_world   = nullptr;
    m_inputHistory.clear();
    m_pendingInputs.clear();
}

void BattleNetSync::setLocalInputMask(std::uint32_t mask)
{
    m_localInputMask = mask;
}

void BattleNetSync::update(float delta)
{
    if (!m_running || !m_world)
        return;

    m_accumulator += delta;
    while (m_accumulator >= FIXED_DT)
    {
        m_accumulator -= FIXED_DT;
        predictStep();
    }
}

void BattleNetSync::predictStep()
{
    ++m_clientFrame;

    auto director     = m_world->getDirector();
    auto directorComp = MG_GET_COMPONENT(director, DirectorComponent);
    auto localPlayer  = m_world->ecsManager.getEntity(directorComp->localPlayerEntityId);
    if (localPlayer)
    {
        auto inputComp = MG_GET_COMPONENT(localPlayer, InputComponent);
        if (inputComp)
            inputComp->keyDown = m_localInputMask;
    }

    m_world->update(FIXED_DT);

    m_inputHistory.push_back({m_clientFrame, m_localInputMask});
    if (m_inputHistory.size() > MAX_INPUT_HISTORY)
        m_inputHistory.pop_front();

    m_pendingInputs.push_back({m_clientFrame, m_localInputMask});
}

void BattleNetSync::applySnapshot(const std::string& worldDump,
                                  std::uint32_t serverFrame,
                                  std::uint32_t lastProcessedClientFrame)
{
    if (!m_running || !m_world)
        return;

    if (serverFrame <= m_lastServerFrame)
        return;

    mugen::ByteBuffer buffer(const_cast<std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(worldDump.data())),
                             static_cast<std::uint32_t>(worldDump.size()));
    if (!m_world->deserialize(buffer))
    {
        MG_LOG_E("BattleNetSync: deserialize snapshot failed");
        return;
    }
    // GameWord::deserialize → postDeserializeInit → BehaviorTreeSystem::onEntityAdded
    // 对无 root 的实体重建 BT；SkillCast/BT 组件态保留，AttackAction enter 幂等续跑
    m_lastServerFrame = serverFrame;

    m_world->bindLocalPlayer(static_cast<mugen::EntityId>(m_localActorId));

    while (!m_inputHistory.empty() && m_inputHistory.front().frame <= lastProcessedClientFrame)
        m_inputHistory.pop_front();
    while (!m_pendingInputs.empty() && m_pendingInputs.front().frame <= lastProcessedClientFrame)
        m_pendingInputs.pop_front();

    auto director     = m_world->getDirector();
    auto directorComp = MG_GET_COMPONENT(director, DirectorComponent);
    if (!directorComp)
    {
        MG_LOG_E("BattleNetSync: snapshot missing DirectorComponent, skip replay");
        return;
    }

    auto localPlayer = m_world->ecsManager.getEntity(directorComp->localPlayerEntityId);
    auto inputComp   = localPlayer ? MG_GET_COMPONENT(localPlayer, InputComponent) : nullptr;
    if (!inputComp)
    {
        // 本地实体/输入组件缺失时，至少用当前输入把未确认帧推完，避免客户端停滞在服务器过去状态
        MG_LOG_E("BattleNetSync: local player input unavailable, replay with current mask");
        for ([[maybe_unused]] const auto& cmd : m_inputHistory)
        {
            m_world->update(FIXED_DT);
        }
        return;
    }

    for (const auto& cmd : m_inputHistory)
    {
        inputComp->keyDown = cmd.mask;
        m_world->update(FIXED_DT);
    }
}

bool BattleNetSync::consumePendingInput(PendingInput& out)
{
    if (m_pendingInputs.empty())
        return false;

    out = m_pendingInputs.front();
    m_pendingInputs.pop_front();
    return true;
}

}  // namespace gameui
