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
    int32_t ruleId        = 0;
    int32_t subType       = -1;
    int32_t remainingMs   = 0;
    int32_t repeatCount   = 1;
    int32_t stacks        = 1;  // 兼容旧字段，与 repeatCount 同步
    int32_t sourceSkillId = 0;
    int32_t tickAccumMs   = 0;
    int32_t innerCdMs     = 0;
    int32_t level         = 1;
    float appliedValue    = 0.0f;  // 规则已写入扩展属性/缩放的量（叠层刷新用）
    bool applied          = false;
    int32_t vfxEntityId   = 0;     // Buff Spine 实体

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
                           vfxEntityId);
};

class BuffComponent : public Component
{
public:
    typedef Component Super;

    BuffComponent() {}
    virtual ~BuffComponent() {}

    std::vector<BuffInstance> buffs;

    // 引用计数：>0 时无敌 / 霸体生效
    int32_t invincibleRef  = 0;
    int32_t superArmorRef  = 0;

    MG_DEFINE_SERIALIZABLE(buffs, invincibleRef, superArmorRef);
};

NS_MG_END
