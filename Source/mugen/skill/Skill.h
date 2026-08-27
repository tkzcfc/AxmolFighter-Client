#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

class SkillAttackConfig;

// 单条技能运行时：CD / 次数 / 消耗缩放活在对象上。管道与朝向是当前施法会话，在 SkillManager。
class Skill : public Object
{
public:
    typedef Object Super;

public:
    Skill() {}

    virtual ~Skill() {}

    const char* typeName() const { return "Skill"; }

    void bindFromConfig(const SkillAttackConfig* cfg);

    void tick(int32_t dtMs);

public:
    int32_t skillAttackId     = 0;
    int32_t nextSkillAttackId = -1;
    int32_t level             = 1;
    int32_t coolDownMs        = 0;
    int32_t coolDownMaxMs     = 0;
    int32_t releaseCount      = 1;
    int32_t releaseMax        = 1;
    float coldTimeScale       = 1.0f;
    float mpConsumeScale      = 1.0f;
    float epConsumeScale      = 1.0f;

public:
    MG_DEFINE_SERIALIZABLE(skillAttackId,
                           nextSkillAttackId,
                           level,
                           coolDownMs,
                           coolDownMaxMs,
                           releaseCount,
                           releaseMax,
                           coldTimeScale,
                           mpConsumeScale,
                           epConsumeScale)
};

NS_MG_END
