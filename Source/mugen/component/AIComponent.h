#pragma once

#include "mugen/ai/AiAgent.h"
#include "mugen/core/ecs/Component.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class AIComponent : public Component
{
public:
    typedef Component Super;

public:
    AIComponent() {}

    virtual ~AIComponent() {}

    AiAgent* ensureAgent();

    void restoreRuntimeData();

private:
    std::vector<uint8_t> m_pendingRuntime;

public:
    std::unique_ptr<AiAgent> agent;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
