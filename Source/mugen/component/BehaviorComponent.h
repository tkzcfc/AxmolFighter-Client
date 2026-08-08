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

    // 当前技能所属输入槽 / 同槽连段步
    int32_t activeInputSlot   = 0;
    int32_t activeStepInSlot  = 0;

    // Behavior 受击硬直剩余时间（毫秒）
    int32_t hitStunRemainingMs = 0;

    // 取消窗口
    bool interruptOpen      = false;
    bool interruptExtraOpen = false;

    // 跑中突刺技能（黑月 Y 槽简化：通常为 defaultSkillIds[1]）
    int32_t thrustSkillAttackId = 0;

    // —— 移动 / 跳跃 / 跑 ——
    // true=需双击同向才跑（对齐黑月默认）；false=按住即跑
    bool clickToWalk = true;
    // 上次按下移动方向的象限：1=右 2=上 3=左 4=下；0=无
    int32_t lastMoveQuadrant = 0;
    // 上次按下移动方向的时间戳（ms，相对 ECS runningTime）
    int64_t lastMovePressMs = 0;
    // 落地锁输入剩余时间
    int32_t landLockMs = 0;
    // 倒地躺地 / 起身剩余时间
    int32_t downRemainMs  = 0;
    int32_t getUpRemainMs = 0;

    MG_DEFINE_SERIALIZABLE(statusTags,
                           currentBranchIndex,
                           currentKind,
                           activeSkillAttackId,
                           pendingSkillAttackId,
                           activeInputSlot,
                           activeStepInSlot,
                           hitStunRemainingMs,
                           interruptOpen,
                           interruptExtraOpen,
                           thrustSkillAttackId,
                           clickToWalk,
                           lastMoveQuadrant,
                           lastMovePressMs,
                           landLockMs,
                           downRemainMs,
                           getUpRemainMs);
};

NS_MG_END
