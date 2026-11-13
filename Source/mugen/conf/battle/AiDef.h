#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf::battle
{
// 怪物决策配置，由 ai.lua 降级（见 DESIGN.md §9.1）。
// 字段与源表 AiConfig 一一对应；monster AI 只读这张新表。
struct AiDef
{
    // 警戒进入延迟范围。
    std::vector<int32_t> m_alertDelayTime;
    // 追击进入延迟范围。
    std::vector<int32_t> m_chaseDelayTime;
    // 追击生效的横向 / 纵深区间。
    std::vector<int32_t> m_chaseScopeX;
    std::vector<int32_t> m_chaseScopeZ;
    // 曝气技能组及其 SkillAi 引用。
    std::vector<std::vector<int32_t>> m_crazySkillAiIds;
    std::vector<std::vector<int32_t>> m_crazySkillIds;
    // 配置行 id（源表 id）。
    int32_t m_id = 0;
    // 摇杆指令技能组。
    std::vector<std::vector<int32_t>> m_joystickSkillIds;
    // 特殊技能组（醒技能 / 待机技能）。
    std::vector<std::vector<int32_t>> m_otherSkillIds;
    // 巡逻进入延迟范围。
    std::vector<int32_t> m_patrolDelayTime;
    // 巡逻随机偏移的横向 / 纵深区间。
    std::vector<int32_t> m_patrolScopeX;
    std::vector<int32_t> m_patrolScopeZ;
    // 各技能槽的 SkillAi 引用。
    std::vector<std::vector<int32_t>> m_skillAiIds;
    // 普通技能组（按槽位）。
    std::vector<std::vector<int32_t>> m_skillIds;
    // 全局施法间隔（毫秒）。
    int32_t m_skillInterval = 0;
    // 各槽优先级。
    std::vector<std::vector<int32_t>> m_skillPriorityLevel;
    // 各槽施法间隔（毫秒）。
    std::vector<std::vector<int32_t>> m_skillPriorityLevelCd;
    // 索敌的横向 / 纵深区间。
    std::vector<int32_t> m_targetScopeX;
    std::vector<int32_t> m_targetScopeZ;

    MG_REFLECT(AiDef,
               m_alertDelayTime,
               m_chaseDelayTime,
               m_chaseScopeX,
               m_chaseScopeZ,
               m_crazySkillAiIds,
               m_crazySkillIds,
               m_id,
               m_joystickSkillIds,
               m_otherSkillIds,
               m_patrolDelayTime,
               m_patrolScopeX,
               m_patrolScopeZ,
               m_skillAiIds,
               m_skillIds,
               m_skillInterval,
               m_skillPriorityLevel,
               m_skillPriorityLevelCd,
               m_targetScopeX,
               m_targetScopeZ)
};
}  // namespace mugen::conf::battle