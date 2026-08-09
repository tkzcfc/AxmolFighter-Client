#pragma once

#include "mugen/core/ecs/Component.h"

#include <vector>

NS_MG_BEGIN

/** 技能施法上下文（对齐黑月 SkillBase / SkillPool 运行时字段） */
class SkillCastComponent : public Component
{
public:
    typedef Component Super;

    SkillCastComponent() {}
    virtual ~SkillCastComponent() {}

    // 当前 / 预输入技能
    int32_t activeSkillAttackId  = 0;
    int32_t pendingSkillAttackId = 0;

    // 输入槽 / 同槽连段步
    int32_t activeInputSlot  = 0;
    int32_t activeStepInSlot = 0;

    // 取消窗
    bool interruptOpen      = false;
    bool interruptExtraOpen = false;

    // 跑中突刺
    int32_t thrustSkillAttackId = 0;

    // 预输入技能的槽/步（勿覆盖 active*，否则当前 CondAttackSlot 会断）
    int32_t pendingInputSlot  = 0;
    int32_t pendingStepInSlot = 0;

    // 跑取消请求（对齐黑月 bNextRunStatus）
    bool wantRunCancel = false;

    // 管道三态（黑月多段充能）
    int32_t pipeIndex       = 0;
    int32_t prePipeIndex    = 0;
    int32_t expectPipeIndex = 0;
    int32_t releaseCount    = 0;
    int32_t towardIndex     = 1;

    // castBegan 幂等
    bool costPaid             = false;
    int32_t costPaidPipeIndex = -1;

    // 近战多段命中追踪（可序列化，替代 CombatSystem 静态 map）
    int32_t meleeHitSkillId = 0;
    std::vector<uint32_t> meleeHitTargetIds;
    std::vector<int32_t> meleeHitCounts;
    std::vector<int32_t> meleeHitCooldowns;

    MG_DEFINE_SERIALIZABLE(activeSkillAttackId,
                           pendingSkillAttackId,
                           activeInputSlot,
                           activeStepInSlot,
                           interruptOpen,
                           interruptExtraOpen,
                           thrustSkillAttackId,
                           pendingInputSlot,
                           pendingStepInSlot,
                           wantRunCancel,
                           pipeIndex,
                           prePipeIndex,
                           expectPipeIndex,
                           releaseCount,
                           towardIndex,
                           costPaid,
                           costPaidPipeIndex,
                           meleeHitSkillId,
                           meleeHitTargetIds,
                           meleeHitCounts,
                           meleeHitCooldowns);
};

NS_MG_END
