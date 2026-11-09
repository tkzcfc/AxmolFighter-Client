#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf::battle
{
// 技能施放配置，由 skill.lua 降级（见 DESIGN.md §6.1）。
struct SkillDef
{
    // 配置行 id（源表 id）。
    int32_t id = 0;
    // 每个方向列是一串动作片段 id（对应 actionIds[toward][i]，指向 battle_clip）。
    std::vector<std::vector<int32_t>> sequences;
    // 抬起释放（蓄力）时播放的动作列（upActionIds，与 sequences 同形状，指向 battle_clip）。
    // 仅 holdRelease 为真时使用。
    std::vector<std::vector<int32_t>> releaseSequences;
    // 是否抬起释放（蓄力）技能（skillType == 1）。
    bool holdRelease = false;
    // 抬起释放判定时长（毫秒，pressTime）；按住时间小于该值才采用 releaseSequences。
    int32_t pressTimeMs = 0;
    // 连段下一步技能 id（nextSkill）。
    int32_t next = 0;
    // 冷却与 PVP 冷却（毫秒，cd / pvpCd）。
    int64_t cdMs = 0;
    int64_t pvpCdMs = 0;
    // 次数制次数（cdCount，-1 表示单次）。
    int32_t charges = 0;
    // 消耗：MP / EP / 晶体 / 怒气。
    int32_t costMp = 0;
    int32_t costEp = 0;
    int32_t costCrystal = 0;
    int32_t costRage = 0;
    // 打断优先级（sorder）。
    int32_t sorder = 0;
    // 无视优先级的位掩码（sorderControlType）。
    std::vector<int32_t> sorderControl;
    // SkillAi 闸门配置（skillAiId）。
    int32_t aiGate = 0;
    // 技能类型（type，槽位分类；-1 表示普通技能）。
    int32_t type = 0;

    MG_REFLECT(SkillDef,
               id,
               sequences,
               releaseSequences,
               holdRelease,
               pressTimeMs,
               next,
               cdMs,
               pvpCdMs,
               charges,
               costMp,
               costEp,
               costCrystal,
               costRage,
               sorder,
               sorderControl,
               aiGate,
               type)
};
}  // namespace mugen::conf::battle
