#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/TableConfig.h"

#include <vector>

NS_MG_BEGIN

struct SkillDeckEntry
{
    int32_t skillAttackId     = 0;
    int32_t nextSkillAttackId = -1;
    int32_t level             = 1;
    int32_t coolDownMs        = 0;
    int32_t coolDownMaxMs     = 0;  // 表 cd 基准；开 CD 时 × coldTimeScale
    int32_t releaseCount      = 1;
    int32_t releaseMax        = 1;
    float coldTimeScale       = 1.0f;
    float mpConsumeScale      = 1.0f;
};

class SkillDeckComponent : public Component
{
public:
    typedef Component Super;

    SkillDeckComponent() {}
    virtual ~SkillDeckComponent() {}

    std::vector<SkillDeckEntry> skills;
    // 与 SkillBar.skillSlots 按下标对齐：每个槽对应 skills 下标列表
    std::vector<std::vector<int32_t>> slotSkillIndices;
    int32_t nextSkillAttackId = 0;

    MG_DEFINE_SERIALIZABLE(nextSkillAttackId);
};

NS_MG_END
