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
    int32_t slot = 0;
    // 技能配置标识。
    int32_t configId = -1;
    // 槽位链内的步骤序号（1 起）。
    int32_t step = 1;
    // 槽位链内的总步骤数。
    int32_t stepCount = 1;
    // 同槽多链的链序号（1 起）。
    int32_t link = 1;
    // 同链多模式的模式号（1 起；`B_2` 为 2）。
    int32_t mode = 1;
    // 剩余冷却时间。
    Fixed cd;
    // 当前冷却周期最大时间。
    Fixed coldMaxTime;
    // 当前可用施放次数。
    int32_t releaseCount = 1;
    // 可累积的最大施放次数。
    int32_t releaseMax = 1;
    // 是否正在推进冷却计时。
    bool timing = false;
    // 是否已是不可续接的末段。
    bool finally = false;
    MG_REFLECT(SkillRuntime, slot, configId, step, stepCount, link, mode, cd, coldMaxTime, releaseCount,
               releaseMax, timing, finally)
};
// 一条技能修正，按来源实例撤销。
struct SkillMod
{
    // SkillModField 数值。
    int32_t field = 0;
    // SkillScope 数值。
    int32_t scope = 0;
    // 槽位下标；非逐槽时为 −1。
    int32_t slot = -1;
    // 已求值的增量。
    Fixed value;
    // 来源实例序号。
    uint32_t source = 0;
    MG_REFLECT(SkillMod, field, scope, slot, value, source)
};
// 角色持有的全部技能运行数据（槽位名 + 技能运行数据）。
struct SkillBookComponent
{
    // 槽位名（A、Y、F、E、B、C、D…）。
    std::vector<std::string> slots;
    // 全部技能运行数据（按槽位 + 链内顺序）。
    std::vector<SkillRuntime> skills;
    // 技能级修正。
    std::vector<SkillMod> mods;
    // 查找指定槽位名、步骤、链与模式的技能下标；不存在返回 kNoSkill。
    int32_t find(std::string_view slot, int32_t step, int32_t link = 1, int32_t mode = 1) const;
    MG_REFLECT(SkillBookComponent, slots, skills, mods)
};
// 当前施法状态（当前技能、缓存下一招、打断窗口）。
struct SkillCastComponent
{
    // 当前施法技能下标，kNoSkill 表示未施法。
    int32_t skill = kNoSkill;
    // 缓存的下一次施放技能下标。
    int32_t nextSkill = kNoSkill;
    // 技能结束后是否接续跑动。
    bool nextRun = false;
    // 是否允许普通优先级打断。
    bool allowInterrupt = true;
    // 是否允许额外优先级打断。
    bool allowExtraInterrupt = false;
    // 取消窗口已接受切招：下一帧 FSM 在 Attack 内结束旧施法并开始新施法。
    bool switched = false;
    // 是否被强制打断。
    bool interrupted = false;
    // 是否处于曝气。
    bool crazy = false;
    // 当前施法管线待领取的伙伴怒气。
    Fixed partnerRagePendingGain;
    // 待领取怒气是否有效。
    bool partnerRagePendingGainActive = false;
    // 冻结结束后等待回退变身。
    bool partnerRageRecoverPending = false;
    MG_REFLECT(SkillCastComponent, skill, nextSkill, nextRun, allowInterrupt, allowExtraInterrupt, switched,
               interrupted, crazy, partnerRagePendingGain, partnerRagePendingGainActive,
               partnerRageRecoverPending)
};
// 角色 / 特效的动作时间轴（片段列表与逐片段运行状态）。
struct ActionTimelineComponent
{
    // 当前技能或特效的片段 id 列表。
    std::vector<int32_t> clips;
    // 当前片段下标。
    int32_t index = 0;
    // 当前片段已运行毫秒。
    Fixed clipMs;
    // 当前片段的循环次数要求（-1 无限）。
    Fixed loops = Fixed::fromInt(1);
    // 当前片段的尾延时（毫秒）。
    int32_t tailMs = 0;
    // 当前片段动画是否已完成。
    bool animationEnd = false;
    // 时间轴是否已全部完成。
    bool finished = false;
    // 下一 cue 游标（cue 按帧升序）。
    int32_t nextCue = 0;
    // 当前片段已生成的特效（autoRelease=0 时退出清理）。
    std::vector<Entity> spawnedEffects;
    // 当前片段 id（供摘要 action 行使用）。
    int32_t clipId = 0;
    // 刚进入的片段要求下一次时间轴遍历不推进。
    bool deferAdvance = false;
    // 本片段是否已触发变身 cue。
    bool transformFired = false;
    // 本片段是否已触发召唤 cue。
    bool summoned = false;
    // 定身正在生效（复位前不再重复施加）。
    bool timeStopOn = false;
    // 定身可再次触发的世界时间（毫秒）。
    Fixed timeStopResetAtMs;
    MG_REFLECT(ActionTimelineComponent, clips, index, clipMs, loops, tailMs, animationEnd, finished, nextCue,
               spawnedEffects, clipId, deferAdvance, transformFired, summoned, timeStopOn, timeStopResetAtMs)
};
}  // namespace mugen::sim
