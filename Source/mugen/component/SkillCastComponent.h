#pragma once

#include "mugen/core/ecs/Component.h"

#include <vector>

NS_MG_BEGIN

/** 技能施法上下文（SkillBase / SkillPool 运行时字段） */
class SkillCastComponent : public Component
{
public:
    typedef Component Super;

    SkillCastComponent() {}
    virtual ~SkillCastComponent() {}

    // 当前 / 预输入技能
    int32_t activeSkillAttackId  = 0;
    int32_t pendingSkillAttackId = 0;

    // 输入槽 / 同槽连段步 / 模式
    int32_t activeInputSlot  = 0;
    int32_t activeStepInSlot = 0;
    int32_t modeIndex        = 0;  // Mode 层；当前固定 0

    // 取消窗
    bool interruptOpen      = false;
    bool interruptExtraOpen = false;

    // 跑中突刺
    int32_t thrustSkillAttackId = 0;

    // 特殊技：闪避 / 爆气（不占数字键槽时由专用键触发）
    int32_t dodgeSkillAttackId = 0;
    int32_t crazySkillAttackId = 0;
    bool crazyActive           = false;
    int32_t crazyRemainMs      = 0;

    // 预输入技能的槽/步（勿覆盖 active*，否则当前 CondAttackSlot 会断）
    int32_t pendingInputSlot  = 0;
    int32_t pendingStepInSlot = 0;

    // 跑取消请求（bNextRunStatus）
    bool wantRunCancel = false;

    // 管道三态（多段充能）
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

    // 当前动作生成的特效实体（autoRelease==0 时 exit 销毁）
    std::vector<uint32_t> spawnedEffectIds;

    // 输入缓冲（activation overlay：tags 未过时暂存，窗口内再尝试 preset）
    int32_t bufferSkillAttackId = 0;
    int32_t bufferInputSlot     = 0;
    int32_t bufferStepInSlot    = 0;
    int32_t bufferRemainMs      = 0;
    uint32_t bufferReleaseTags  = 0;

    MG_DEFINE_SERIALIZABLE(activeSkillAttackId,
                           pendingSkillAttackId,
                           activeInputSlot,
                           activeStepInSlot,
                           modeIndex,
                           interruptOpen,
                           interruptExtraOpen,
                           thrustSkillAttackId,
                           dodgeSkillAttackId,
                           crazySkillAttackId,
                           crazyActive,
                           crazyRemainMs,
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
                           meleeHitCooldowns,
                           spawnedEffectIds,
                           bufferSkillAttackId,
                           bufferInputSlot,
                           bufferStepInSlot,
                           bufferRemainMs,
                           bufferReleaseTags);
};

NS_MG_END
