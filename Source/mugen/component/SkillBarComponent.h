#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/Config.h"

NS_MG_BEGIN

class SkillSlotItem : public Object
{
public:
    typedef Object Super;

public:
    SkillSlotItem() {}

    virtual ~SkillSlotItem() {}

    // 技能槽位索引
    int32_t slotIndex = 0;
    // 技能索引列表
    std::vector<int32_t> skillIndexs;

    MG_DEFINE_SERIALIZABLE(slotIndex, skillIndexs);
};

class SkillBarComponent : public Component
{
public:
    typedef Component Super;

public:
    SkillBarComponent() {}
    virtual ~SkillBarComponent() {}

public:
    // 技能栏位映射
    std::vector<SkillSlotItem> skillSlots;

    MG_DEFINE_SERIALIZABLE(skillSlots)
};

NS_MG_END
