#pragma once

#include "mugen/core/ecs/Component.h"

#include <vector>

NS_MG_BEGIN

// 卡组一条：只存拓扑。CD / 次数 / 消耗缩放在 Skill 上。
class SkillDeckEntry : public Object
{
public:
    typedef Object Super;

    SkillDeckEntry() {}
    virtual ~SkillDeckEntry() {}

    int32_t skillAttackId     = 0;
    int32_t nextSkillAttackId = -1;
    int32_t level             = 1;

    MG_DEFINE_SERIALIZABLE(skillAttackId, nextSkillAttackId, level);
};

// 卡组拓扑：技能 id 与槽下标。运行时 CD 不在这里。
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

    MG_DEFINE_SERIALIZABLE(skills, slotSkillIndices, nextSkillAttackId);
};

NS_MG_END
