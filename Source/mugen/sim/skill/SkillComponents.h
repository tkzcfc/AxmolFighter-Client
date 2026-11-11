#pragma once
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/ecs/Entity.h"
#include <string>
#include <vector>

namespace mugen::sim
{
// 无效的技能下标。
inline constexpr int32_t kNoSkill = -1;
// 单个技能步骤的运行数据（冷却、次数）。
struct SkillRuntime
{
    // 所属槽位下标。
    int32_t m_slot = 0;
    // 技能配置标识。
    int32_t m_configId = -1;
    // 槽位链内的步骤序号（1 起）。
    int32_t m_step = 1;
    // 槽位链内的总步骤数。
    int32_t m_stepCount = 1;
    // 剩余冷却时间。
    Fixed m_cd;
    // 当前冷却周期最大时间。
    Fixed m_coldMaxTime;
    // 当前可用施放次数。
    int32_t m_releaseCount = 1;
    // 可累积的最大施放次数。
    int32_t m_releaseMax = 1;
    // 是否正在推进冷却计时。
    bool m_timing = false;
    // 是否已是不可续接的末段。
    bool m_finally = false;
    MG_REFLECT(SkillRuntime, m_slot, m_configId, m_step, m_stepCount, m_cd, m_coldMaxTime, m_releaseCount, m_releaseMax,
               m_timing, m_finally)
};
// 一条技能修正，按来源实例撤销。
struct SkillMod
{
    // SkillModField 数值。
    int32_t m_field = 0;
    // SkillScope 数值。
    int32_t m_scope = 0;
    // 槽位下标；非逐槽时为 −1。
    int32_t m_slot = -1;
    // 已求值的增量。
    Fixed m_value;
    // 来源实例序号。
    uint32_t m_source = 0;
    MG_REFLECT(SkillMod, m_field, m_scope, m_slot, m_value, m_source)
};
// 角色持有的全部技能运行数据（槽位名 + 技能运行数据）。
struct SkillBook
{
    // 槽位名（A、Y、F、E、B、C、D…）。
    std::vector<std::string> m_slots;
    // 全部技能运行数据（按槽位 + 链内顺序）。
    std::vector<SkillRuntime> m_skills;
    // 技能级修正。
    std::vector<SkillMod> m_mods;
    // 查找指定槽位名、步骤的技能下标；不存在返回 kNoSkill。
    int32_t find(std::string_view slot, int32_t step) const;
    MG_REFLECT(SkillBook, m_slots, m_skills, m_mods)
};
// 当前施法状态（当前技能、缓存下一招、打断窗口）。
struct SkillCast
{
    // 当前施法技能下标，kNoSkill 表示未施法。
    int32_t m_skill = kNoSkill;
    // 缓存的下一次施放技能下标。
    int32_t m_nextSkill = kNoSkill;
    // 技能结束后是否接续跑动。
    bool m_nextRun = false;
    // 是否允许普通优先级打断。
    bool m_allowInterrupt = true;
    // 是否允许额外优先级打断。
    bool m_allowExtraInterrupt = false;
    // 是否被强制打断。
    bool m_interrupted = false;
    // 是否处于曝气。
    bool m_crazy = false;
    MG_REFLECT(SkillCast, m_skill, m_nextSkill, m_nextRun, m_allowInterrupt, m_allowExtraInterrupt, m_interrupted,
               m_crazy)
};
// 角色 / 特效的动作时间轴（片段列表与逐片段运行状态）。
struct ActionTimeline
{
    // 当前技能或特效的片段 id 列表。
    std::vector<int32_t> m_clips;
    // 当前片段下标。
    int32_t m_index = 0;
    // 当前片段已运行毫秒。
    Fixed m_clipMs;
    // 当前片段的循环次数要求（-1 无限）。
    Fixed m_loops = Fixed::fromInt(1);
    // 当前片段的尾延时（毫秒）。
    int32_t m_tailMs = 0;
    // 当前片段动画是否已完成。
    bool m_animationEnd = false;
    // 时间轴是否已全部完成。
    bool m_finished = false;
    // 下一 cue 游标（cue 按帧升序）。
    int32_t m_nextCue = 0;
    // 当前片段已生成的特效（autoRelease=0 时退出清理）。
    std::vector<Entity> m_spawnedEffects;
    // 当前片段 id（供摘要 action 行使用）。
    int32_t m_clipId = 0;
    // 刚进入的片段要求下一次时间轴遍历不推进。
    bool m_deferAdvance = false;
    MG_REFLECT(ActionTimeline, m_clips, m_index, m_clipMs, m_loops, m_tailMs, m_animationEnd, m_finished, m_nextCue,
               m_spawnedEffects, m_clipId, m_deferAdvance)
};
}  // namespace mugen::sim
