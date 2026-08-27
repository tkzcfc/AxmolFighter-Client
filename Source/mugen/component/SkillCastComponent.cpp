#include "mugen/component/SkillCastComponent.h"

#include "mugen/core/serialize/ByteBuffer.h"

NS_MG_BEGIN

SkillManager* SkillCastComponent::ensureManager()
{
    if (!manager)
        manager = std::make_unique<SkillManager>();
    return manager.get();
}

void SkillCastComponent::restoreRuntimeData()
{
    if (!manager || m_pendingRuntime.empty())
        return;
    ByteBuffer inner(m_pendingRuntime.data(), static_cast<uint32_t>(m_pendingRuntime.size()));
    manager->deserialize(inner);
    m_pendingRuntime.clear();
}

void SkillCastComponent::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    ByteBuffer inner;
    if (manager)
        manager->serialize(inner);
    inner.writeFinish();
    std::vector<uint8_t> blob;
    if (inner.data() && inner.len() > 0)
        blob.assign(inner.data(), inner.data() + inner.len());
    byteBuffer.writeValue(blob);
}

bool SkillCastComponent::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    m_pendingRuntime.clear();
    return byteBuffer.getValue(m_pendingRuntime);
}

NS_MG_END
