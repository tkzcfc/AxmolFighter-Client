#include "ui/battle/OnlineBattleMode.h"

#include "mugen/GameWord.h"
#include "mugen/core/MacroDefinition.h"
#include "net/client_battle.pb.h"

#include <axmol.h>

using namespace mugen;

namespace gameui
{

OnlineBattleMode::OnlineBattleMode(BattleBootParams boot) : m_boot(std::move(boot))
{
    m_netSync = std::make_unique<BattleNetSync>();
}

OnlineBattleMode::~OnlineBattleMode() = default;

bool OnlineBattleMode::init(mugen::GameWord* gameWord)
{
    m_gameWord = gameWord;

    mugen::ByteBuffer buffer(const_cast<std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(m_boot.worldDump.data())),
                             static_cast<std::uint32_t>(m_boot.worldDump.size()));
    if (!m_gameWord->deserialize(buffer))
    {
        MG_LOG_E("OnlineBattleMode: deserialize boot world dump failed");
        return false;
    }

    m_gameWord->bindLocalPlayer(static_cast<mugen::EntityId>(m_boot.actorEntityId));
    m_netSync->start(m_gameWord, m_boot.battleId, m_boot.serverFrame, m_boot.actorEntityId);

    this->listenPush([this](PB::Battle::BattleSnapshotPush& push) { onSnapshot(push); });

    return true;
}

void OnlineBattleMode::onUpdate(float delta)
{
    if (m_netSync)
    {
        m_netSync->setLocalInputMask(m_inputMask);
        m_netSync->update(delta);
        sendPendingInput();
    }
}

void OnlineBattleMode::setInput(uint32_t slot, bool pressed)
{
    if (pressed)
        MG_BIT_SET(m_inputMask, 1 << slot);
    else
        MG_BIT_REMOVE(m_inputMask, 1 << slot);
}

void OnlineBattleMode::onExit()
{
    if (m_netSync)
        m_netSync->stop();
}

void OnlineBattleMode::sendPendingInput()
{
    BattleNetSync::PendingInput pending;
    while (m_netSync->consumePendingInput(pending))
    {
        PB::Battle::BattleInputPush input;
        input.set_battle_id(m_netSync->battleId());
        input.set_client_frame(pending.frame);
        input.set_input_mask(pending.mask);
        input.set_client_time_ms(static_cast<std::uint64_t>(ax::utils::getTimeInMilliseconds()));
        this->notify(input);
    }
}

void OnlineBattleMode::onSnapshot(const PB::Battle::BattleSnapshotPush& push)
{
    if (!m_netSync)
        return;

    m_netSync->applySnapshot(push.world_dump(), push.server_frame(), push.last_processed_client_frame());
}

}  // namespace gameui
