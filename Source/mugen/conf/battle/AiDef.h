#pragma once

#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf::battle
{
// 怪物决策配置，由 ai.lua 降级（见 DESIGN.md §9.1）。
// 字段与源表 AiConfig 一一对应；monster AI 只读这张新表。
struct AiDef
{
    // 警戒进入延迟范围。
    std::vector<int32_t> alertDelayTime;
    // 追击进入延迟范围。
    std::vector<int32_t> chaseDelayTime;
    // 追击生效的横向 / 纵深区间。
    std::vector<int32_t> chaseScopeX;
    std::vector<int32_t> chaseScopeZ;
    // 曝气技能组及其 SkillAi 引用。
    std::vector<std::vector<int32_t>> crazySkillAiIds;
    std::vector<std::vector<int32_t>> crazySkillIds;
    // 配置行 id（源表 id）。
    int32_t id = 0;
    // 摇杆指令技能组。
    std::vector<std::vector<int32_t>> joystickSkillIds;
    // 特殊技能组（醒技能 / 待机技能）。
    std::vector<std::vector<int32_t>> otherSkillIds;
    // 巡逻进入延迟范围。
    std::vector<int32_t> patrolDelayTime;
    // 巡逻随机偏移的横向 / 纵深区间。
    std::vector<int32_t> patrolScopeX;
    std::vector<int32_t> patrolScopeZ;
    // 各技能槽的 SkillAi 引用。
    std::vector<std::vector<int32_t>> skillAiIds;
    // 普通技能组（按槽位）。
    std::vector<std::vector<int32_t>> skillIds;
    // 全局施法间隔（毫秒）。
    int32_t skillInterval = 0;
    // 各槽优先级。
    std::vector<std::vector<int32_t>> skillPriorityLevel;
    // 各槽施法间隔（毫秒）。
    std::vector<std::vector<int32_t>> skillPriorityLevelCd;
    // 索敌的横向 / 纵深区间。
    std::vector<int32_t> targetScopeX;
    std::vector<int32_t> targetScopeZ;

    MG_REFLECT(AiDef,
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

// 单技能自动施放门槛，由 skill_ai.lua 降级。
struct SkillAiDef
{
    int32_t id       = 0;
    int32_t loadCd   = 0;
    int64_t checkCd  = 0;
    int32_t useCount = -1;
    int32_t prob     = 100;
    std::vector<int32_t> oppDisX;
    std::vector<int32_t> oppDisZ;
    int32_t oppStatus  = -1;
    int32_t oppCombo   = -1;
    int32_t oppSkillId = -1;
    std::vector<int32_t> selfHp;
    int32_t selfStatus = -1;
    std::vector<int32_t> mustAll;
    std::vector<int32_t> mustAny;

    MG_REFLECT(SkillAiDef,
               id,
               loadCd,
               checkCd,
               useCount,
               prob,
               oppDisX,
               oppDisZ,
               oppStatus,
               oppCombo,
               oppSkillId,
               selfHp,
               selfStatus,
               mustAll,
               mustAny)
};
}  // namespace mugen::conf::battle