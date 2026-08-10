#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/math/Vec3.h"

#include <vector>

NS_MG_BEGIN

/** 单条 skill_ai 运行时状态（可序列化，替代静态 CD map） */
class SkillAiRuntimeState : public Object
{
public:
    typedef Object Super;

    SkillAiRuntimeState() {}
    virtual ~SkillAiRuntimeState() {}

    int32_t skillAiId       = 0;
    int32_t remainUseCount  = -1;  // -1 不限；>0 剩余次数
    int32_t loadCdRemainMs  = 0;
    int32_t checkCdRemainMs = 0;

    MG_DEFINE_SERIALIZABLE(skillAiId, remainUseCount, loadCdRemainMs, checkCdRemainMs);
};

/** AI 施法节奏、巡逻/追击意图与 skill_ai 状态 */
class AIComponent : public Component
{
public:
    typedef Component Super;

    AIComponent() {}
    virtual ~AIComponent() {}

    // 对应 AiConfig.skillInterval 的施法间隔剩余
    int32_t castIntervalRemainMs = 0;

    // 当前采用的 AiConfig id（0=未绑定）
    int32_t aiConfigId = 0;

    std::vector<SkillAiRuntimeState> skillAiStates;

    // —— 巡逻 / 移动意图（可序列化）——
    Vector3f spawnPosition;
    int32_t patrolTargetX      = 0;
    int32_t patrolTargetY      = 0;
    int32_t patrolWaitRemainMs = 0;
    /** 0=idle 1=moving 2=waiting */
    int32_t patrolState = 0;
    /** 有效巡逻半径（从 AiConfig.patrolScope 或默认推导） */
    int32_t patrolScope = 0;

    /** AI 本帧移动意图（-1/0/1），由 BehaviorTreeSystem 注入 InputComponent 后清零 */
    int8_t moveDirX = 0;
    int8_t moveDirY = 0;

    int32_t alertRemainMs = 0;
    /** 本轮交战是否已完成警觉；玩家离开 targetScope 后清零 */
    bool alertDone = false;

    MG_DEFINE_SERIALIZABLE(castIntervalRemainMs,
                           aiConfigId,
                           skillAiStates,
                           spawnPosition,
                           patrolTargetX,
                           patrolTargetY,
                           patrolWaitRemainMs,
                           patrolState,
                           patrolScope,
                           moveDirX,
                           moveDirY,
                           alertRemainMs,
                           alertDone);
};

NS_MG_END
