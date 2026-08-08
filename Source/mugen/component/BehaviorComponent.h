#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/TableConfig.h"

NS_MG_BEGIN

class BehaviorComponent : public Component
{
public:
    typedef Component Super;

    BehaviorComponent() {}
    virtual ~BehaviorComponent() {}

    const BehaviorTemplateConfig* behaviorTemplate = nullptr;
    const RoleConfig* roleConfig                   = nullptr;

    // 当前状态位（StateTag 掩码）
    uint32_t statusTags = 0;

    // 当前选中的分支索引（-1 表示无）
    int32_t currentBranchIndex = -1;
    int32_t currentKind        = static_cast<int32_t>(BehaviorKind::kIdle);

    // 攻击中：当前 skillAttackId（0 表示未在攻击）
    int32_t activeSkillAttackId = 0;

    // 预输入下一技能
    int32_t pendingSkillAttackId = 0;

    // Behavior 受击硬直剩余时间（毫秒）
    int32_t hitStunRemainingMs = 0;

    // 取消窗口
    bool interruptOpen      = false;
    bool interruptExtraOpen = false;

    MG_DEFINE_SERIALIZABLE(statusTags,
                           currentBranchIndex,
                           currentKind,
                           activeSkillAttackId,
                           pendingSkillAttackId,
                           hitStunRemainingMs,
                           interruptOpen,
                           interruptExtraOpen);
};

NS_MG_END
