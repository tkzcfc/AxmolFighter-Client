#include "mugen/component/AIComponent.h"

#include "mugen/core/serialize/ByteBuffer.h"

NS_MG_BEGIN

AiAgent* AIComponent::ensureAgent()
{
    if (!agent)
        agent = std::make_unique<AiAgent>();
    return agent.get();
}

void AIComponent::restoreRuntimeData()
{
    if (!agent || m_pendingRuntime.empty())
        return;
    ByteBuffer inner(m_pendingRuntime.data(), static_cast<uint32_t>(m_pendingRuntime.size()));
    agent->deserialize(inner);
    m_pendingRuntime.clear();
}

void AIComponent::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    ByteBuffer inner;
    if (agent)
        agent->serialize(inner);
    inner.writeFinish();
    std::vector<uint8_t> blob;
    if (inner.data() && inner.len() > 0)
        blob.assign(inner.data(), inner.data() + inner.len());
    byteBuffer.writeValue(blob);
}

bool AIComponent::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    m_pendingRuntime.clear();
    return byteBuffer.getValue(m_pendingRuntime);
}

NS_MG_END
