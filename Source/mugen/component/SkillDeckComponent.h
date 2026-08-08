#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/TableConfig.h"

#include <vector>

NS_MG_BEGIN

struct SkillDeckEntry
{
    int32_t skillAttackId = 0;
    int32_t level         = 1;
    int32_t coolDownMs    = 0;
    int32_t coolDownMaxMs = 0;
    int32_t releaseCount  = 1;
    int32_t releaseMax    = 1;
};

class SkillDeckComponent : public Component
{
public:
    typedef Component Super;

    SkillDeckComponent() {}
    virtual ~SkillDeckComponent() {}

    std::vector<SkillDeckEntry> skills;
    // 槽位 -> skills 下标
    std::vector<std::vector<int32_t>> slotSkillIndices;
    int32_t nextSkillAttackId = 0;

    MG_DEFINE_SERIALIZABLE(nextSkillAttackId);
};

NS_MG_END
