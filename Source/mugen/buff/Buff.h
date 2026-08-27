#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

class Entity;

// 单条 buff 运行时：剩余时间 / 叠层 / 规则已写入量活在对象上。
class Buff : public Object
{
public:
    typedef Object Super;

public:
    Buff() {}

    virtual ~Buff() {}

    const char* typeName() const { return "Buff"; }

    void tick(Entity* entity, int32_t dtMs);

public:
    int32_t buffId        = 0;
    int32_t ruleId        = 0;
    int32_t subType       = -1;
    int32_t remainingMs   = 0;
    int32_t repeatCount   = 1;
    int32_t stacks        = 1;
    int32_t sourceSkillId = 0;
    int32_t tickAccumMs   = 0;
    int32_t innerCdMs     = 0;
    int32_t level         = 1;
    float appliedValue    = 0.0f;
    bool applied          = false;
    bool stateValid       = false;
    int32_t vfxEntityId   = 0;
    bool destroyed        = false;

public:
    MG_DEFINE_SERIALIZABLE(buffId,
                           ruleId,
                           subType,
                           remainingMs,
                           repeatCount,
                           stacks,
                           sourceSkillId,
                           tickAccumMs,
                           innerCdMs,
                           level,
                           appliedValue,
                           applied,
                           stateValid,
                           vfxEntityId)
};

NS_MG_END
