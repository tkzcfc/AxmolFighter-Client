#include "mugen/component/EffectComponent.h"

#include "mugen/core/serialize/ByteBuffer.h"

NS_MG_BEGIN

Effect* EffectComponent::ensureEffect()
{
    if (!effect)
        effect = std::make_unique<Effect>();
    return effect.get();
}

void EffectComponent::restoreRuntimeData()
{
    if (!effect || m_pendingRuntime.empty())
        return;
    ByteBuffer inner(m_pendingRuntime.data(), static_cast<uint32_t>(m_pendingRuntime.size()));
    effect->deserialize(inner);
    m_pendingRuntime.clear();
}

void EffectComponent::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    ByteBuffer inner;
    if (effect)
        effect->serialize(inner);
    inner.writeFinish();
    std::vector<uint8_t> blob;
    if (inner.data() && inner.len() > 0)
        blob.assign(inner.data(), inner.data() + inner.len());
    byteBuffer.writeValue(blob);
}

bool EffectComponent::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    m_pendingRuntime.clear();
    return byteBuffer.getValue(m_pendingRuntime);
}

NS_MG_END
