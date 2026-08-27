#pragma once

#include "mugen/skill/SkillManager.h"
#include "mugen/core/ecs/Component.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class SkillCastComponent : public Component
{
public:
    typedef Component Super;

public:
    SkillCastComponent() {}

    virtual ~SkillCastComponent() {}

    SkillManager* ensureManager();

    void restoreRuntimeData();

private:
    std::vector<uint8_t> m_pendingRuntime;

public:
    std::unique_ptr<SkillManager> manager;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
