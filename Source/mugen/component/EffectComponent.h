#pragma once

#include "mugen/effect/Effect.h"
#include "mugen/core/ecs/Component.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class EffectComponent : public Component
{
public:
    typedef Component Super;

public:
    EffectComponent() {}

    virtual ~EffectComponent() {}

    Effect* ensureEffect();

    void restoreRuntimeData();

private:
    std::vector<uint8_t> m_pendingRuntime;

public:
    std::unique_ptr<Effect> effect;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
