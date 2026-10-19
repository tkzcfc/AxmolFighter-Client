#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// ai 表映射的角色决策配置。
struct AiConfig
{
    // 警戒、追击的延迟和坐标范围。
    std::vector<int32_t> alertDelayTime;
    std::vector<int32_t> chaseDelayTime;
    std::vector<int32_t> chaseScopeX;
    std::vector<int32_t> chaseScopeZ;
    // 怒气、摇杆和其他技能组及其决策配置。
    std::vector<std::vector<int32_t>> crazySkillAiIds;
    std::vector<std::vector<int32_t>> crazySkillIds;
    int32_t id = 0;
    std::vector<std::vector<int32_t>> joystickSkillIds;
    std::vector<std::vector<int32_t>> otherSkillIds;
    // 巡逻范围、延迟与普通技能组。
    std::vector<int32_t> patrolDelayTime;
    std::vector<int32_t> patrolScopeX;
    std::vector<int32_t> patrolScopeZ;
    std::vector<std::vector<int32_t>> skillAiIds;
    std::vector<std::vector<int32_t>> skillIds;
    // 全局施法间隔与技能优先级、冷却。
    int32_t skillInterval = 0;
    std::vector<std::vector<int32_t>> skillPriorityLevel;
    std::vector<std::vector<int32_t>> skillPriorityLevelCd;
    std::vector<int32_t> targetScopeX;
    std::vector<int32_t> targetScopeZ;

    MG_REFLECT(AiConfig,
               alertDelayTime,
               chaseDelayTime,
               chaseScopeX,
               chaseScopeZ,
               crazySkillAiIds,
               crazySkillIds,
               id,
               joystickSkillIds,
               otherSkillIds,
               patrolDelayTime,
               patrolScopeX,
               patrolScopeZ,
               skillAiIds,
               skillIds,
               skillInterval,
               skillPriorityLevel,
               skillPriorityLevelCd,
               targetScopeX,
               targetScopeZ)
};
}  // namespace mugen::conf
