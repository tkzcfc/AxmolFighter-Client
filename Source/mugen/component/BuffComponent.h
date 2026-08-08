#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/Object.h"

#include <vector>

NS_MG_BEGIN

class BuffInstance : public Object
{
public:
    typedef Object Super;
    BuffInstance() {}
    virtual ~BuffInstance() {}

    int32_t buffId        = 0;
    int32_t remainingMs   = 0;
    int32_t stacks        = 1;
    int32_t sourceSkillId = 0;
    int32_t tickAccumMs   = 0;

    MG_DEFINE_SERIALIZABLE(buffId, remainingMs, stacks, sourceSkillId, tickAccumMs);
};

class BuffComponent : public Component
{
public:
    typedef Component Super;

    BuffComponent() {}
    virtual ~BuffComponent() {}

    std::vector<BuffInstance> buffs;

    MG_DEFINE_SERIALIZABLE(buffs);
};

NS_MG_END
