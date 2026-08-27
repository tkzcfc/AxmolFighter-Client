#pragma once

#include "mugen/buff/BuffManager.h"
#include "mugen/core/ecs/Component.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class BuffComponent : public Component
{
public:
    typedef Component Super;

public:
    BuffComponent() {}

    virtual ~BuffComponent() {}

    BuffManager* ensureManager();

    void restoreRuntimeData();

private:
    std::vector<uint8_t> m_pendingRuntime;

public:
    std::unique_ptr<BuffManager> manager;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
