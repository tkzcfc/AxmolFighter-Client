#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

class Entity;
class SkillAiConfig;
class Random;

// 单条 skill_ai 运行时：次数与 CD 活在对象上，check 判定本帧是否允许用绑定技能 auto-cast。
class SkillAi : public Object
{
public:
    typedef Object Super;

public:
    SkillAi() {}

    virtual ~SkillAi() {}

    const char* typeName() const { return "SkillAi"; }

    void ensure(const SkillAiConfig* cfg);

    void tick(int32_t dtMs);

    bool check(Entity* self, Entity* target, Random& rng);

public:
    int32_t skillAiId       = 0;
    int32_t remainUseCount  = -1;
    int32_t loadCdRemainMs  = 0;
    int32_t checkCdRemainMs = 0;

public:
    MG_DEFINE_SERIALIZABLE(skillAiId, remainUseCount, loadCdRemainMs, checkCdRemainMs)
};

NS_MG_END
