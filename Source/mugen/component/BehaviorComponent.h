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

    // 当前选中的分支索引（-1 表示无）/ 表现 kind
    int32_t currentBranchIndex = -1;
    int32_t currentKind        = static_cast<int32_t>(BehaviorKind::kIdle);

    // Behavior 受击硬直剩余时间（毫秒）
    int32_t hitStunRemainingMs = 0;

    // —— 移动 / 跳跃 / 跑 ——
    // true=需双击同向才跑（默认）；false=按住即跑
    bool clickToWalk = true;
    // 上次按下移动方向的象限：1=右 2=上 3=左 4=下；0=无
    int32_t lastMoveQuadrant = 0;
    // 上次按下移动方向的时间戳（ms，相对 ECS runningTime）
    int64_t lastMovePressMs = 0;
    // 落地锁输入剩余时间
    int32_t landLockMs = 0;
    // 倒地躺地 / 起身 / 受击过渡剩余时间
    int32_t downRemainMs      = 0;
    int32_t getUpRemainMs     = 0;
    int32_t hitSwitchRemainMs = 0;

    // 定身剩余（ms）：>0 禁移动/禁技能输入消费
    int32_t staticRemainMs = 0;

    MG_DEFINE_SERIALIZABLE(statusTags,
                           currentBranchIndex,
                           currentKind,
                           hitStunRemainingMs,
                           clickToWalk,
                           lastMoveQuadrant,
                           lastMovePressMs,
                           landLockMs,
                           downRemainMs,
                           getUpRemainMs,
                           hitSwitchRemainMs,
                           staticRemainMs);
};

NS_MG_END
