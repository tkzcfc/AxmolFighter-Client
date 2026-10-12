#pragma once
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim
{
/// 无效的技能数组下标。
inline constexpr int32_t kNoSkill = -1;
/// 技能在槽位、连段、模式和步骤中的唯一定位。
struct SkillAddress
{
    /// 技能槽位下标。
    uint16_t m_slot = 0;
    /// 槽位内的连段序号。
    int32_t m_index = 1;
    /// 同一连段可选模式的序号。
    int32_t m_mode = 1;
    /// 当前模式内的步骤序号。
    int32_t m_step = 1;
    friend bool operator==(const SkillAddress&, const SkillAddress&) = default;
    MG_REFLECT(SkillAddress, m_slot, m_index, m_mode, m_step)
};
/// 单个技能的自动施放运行状态。
struct SkillAi
{
    /// AI 配置行 id，-1 表示不受 AI 条件限制。
    int32_t m_configId = -1;
    /// 施放后的装载冷却时间。
    Fixed m_loadCd;
    /// 下次检查条件前的等待时间。
    Fixed m_checkCd;
    /// 剩余可自动施放次数，-1 表示不限次数。
    int32_t m_useCount = -1;
    /// 移动期间累积的距离补偿帧数。
    Fixed m_moveDistanceFrame;
    MG_REFLECT(SkillAi, m_configId, m_loadCd, m_checkCd, m_useCount, m_moveDistanceFrame)
};
/// 单个技能步骤的可序列化运行数据。
struct SkillBase
{
    /// 本步骤的地址。
    SkillAddress m_address;
    /// 技能配置行 id。
    int32_t m_configId = -1;
    /// 当前所属技能组。
    int32_t m_group = 1;
    /// 此技能连段的总步骤数。
    int32_t m_stepCount = 1;
    /// 此技能槽位可用模式数。
    int32_t m_slotCount = 1;
    /// 剩余冷却时间。
    Fixed m_cd;
    /// 当前冷却周期的最大时间。
    Fixed m_coldMaxTime;
    /// 冷却时间缩放。
    Fixed m_coldScale = "1"_fx;
    /// 法力消耗缩放。
    Fixed m_mpScale = "1"_fx;
    /// 能量消耗缩放。
    Fixed m_epScale = "1"_fx;
    /// 额外能量消耗。
    Fixed m_epPlus;
    /// 施放所需水晶数。
    int32_t m_crystal = 0;
    /// 当前可用施放次数。
    int32_t m_releaseCount = 1;
    /// 可累积的最大施放次数。
    int32_t m_releaseMax = 1;
    /// 是否正在推进冷却或技能计时。
    bool m_timing = false;
    /// 是否已收到按键抬起事件。
    bool m_upTouch = false;
    /// 是否已满足长按释放条件。
    bool m_enoughRelease = false;
    /// 本次准备后的技能计时。
    Fixed m_skillTime;
    /// 当前管线编号。
    int32_t m_pipe = 1;
    /// 上一次使用的管线编号。
    int32_t m_previousPipe = 2;
    /// 下次施放预期切换到的管线编号。
    int32_t m_expectedPipe = 1;
    /// 选中的朝向动作列。
    int32_t m_toward = 1;
    /// 输入方向向量编号。
    int32_t m_vector = 1;
    /// 当前朝向可执行动作数。
    int32_t m_actionCount = 0;
    /// 是否已进入不可续接的末段。
    bool m_finally = false;
    /// 当前技能结束后是否接续跑动。
    bool m_nextRun = false;
    /// 已排队的下一技能下标。
    int32_t m_nextSkill = kNoSkill;
    /// 由控制输入传递的方向角。
    std::optional<Fixed> m_controlAngle;
    /// 临时技能组的剩余时间。
    Fixed m_groupTime;
    /// 临时技能变化的剩余时间。
    Fixed m_changeTime;
    /// 本技能的自动施放状态。
    SkillAi m_ai;
    MG_REFLECT(SkillBase, m_address, m_configId, m_group, m_stepCount, m_slotCount,
               m_cd, m_coldMaxTime, m_coldScale, m_mpScale, m_epScale, m_epPlus, m_crystal,
               m_releaseCount, m_releaseMax, m_timing, m_upTouch, m_enoughRelease, m_skillTime,
               m_pipe, m_previousPipe, m_expectedPipe, m_toward, m_vector, m_actionCount,
               m_finally, m_nextRun, m_nextSkill, m_controlAngle, m_groupTime, m_changeTime, m_ai)
};
/// 可启用或禁用的一类技能槽位。
struct SkillSlot
{
    /// 槽位名称。
    std::string m_name;
    /// 是否允许从此槽位选择技能。
    bool m_enabled = true;
    MG_REFLECT(SkillSlot, m_name, m_enabled)
};
/// 技能地址到技能数组下标的查找项。
struct SkillIndex
{
    /// 被索引的技能地址。
    SkillAddress m_address;
    /// 对应的技能数组下标。
    int32_t m_skill = kNoSkill;
    MG_REFLECT(SkillIndex, m_address, m_skill)
};
/// 角色持有的全部技能及其共享运行状态。
struct SkillPool
{
    /// 已加载的技能槽位。
    std::vector<SkillSlot> m_slots;
    /// 按地址查询技能的索引表。
    std::vector<SkillIndex> m_index;
    /// 技能运行数据。
    std::vector<SkillBase> m_skills;
    /// 当前正在施放的技能。
    int32_t m_current = kNoSkill;
    /// 最近一次触发施放事件的技能。
    int32_t m_previous = kNoSkill;
    /// 被中断前保留的技能。
    int32_t m_before = kNoSkill;
    /// 保留技能仍有效的剩余时间。
    Fixed m_beforeTime;
    /// 各自动施放槽位的公共间隔。
    std::vector<Fixed> m_aiSkillCastInterval;
    /// 正在冷却的技能下标。
    std::vector<int32_t> m_running;
    /// 需要对外展示冷却的基础槽位。
    std::vector<uint16_t> m_coolingSlots;
    /// 自动施放条件使用的目标实体。
    Entity m_target;
    /// 是否允许普通优先级打断。
    bool m_allowInterrupt = true;
    /// 是否允许额外优先级打断。
    bool m_allowExtraInterrupt = false;
    /// 当前施放是否切换了槽位。
    bool m_slotChanged = true;
    /// 是否处于爆发状态。
    bool m_crazy = false;
    /// 是否允许冲刺技能替换普通攻击。
    bool m_canSprint = true;
    /// 是否正在恢复爆发资源。
    bool m_angerRecover = false;
    /// 是否由自动施放路径选择技能。
    bool m_autoCast = false;
    /// 当前全局技能组。
    int32_t m_group = 1;
    /// 当前全局技能变化编号。
    int32_t m_change = 1;
    /// 角色持有的水晶数。
    int32_t m_crystals = 0;
    /// 全局法力消耗缩放。
    Fixed m_mpScale = "1"_fx;
    /// 全局能量消耗缩放。
    Fixed m_epScale = "1"_fx;
    /// 全局额外能量消耗。
    Fixed m_epPlus;
    /// 爆发能量衰减缩放。
    Fixed m_cpScale = "1"_fx;
    /// 最近一次控制方向角。
    std::optional<Fixed> m_controlAngle;
    MG_REFLECT(SkillPool, m_slots, m_index, m_skills, m_current, m_previous, m_before, m_beforeTime,
               m_aiSkillCastInterval, m_running, m_coolingSlots, m_target, m_allowInterrupt,
               m_allowExtraInterrupt, m_slotChanged, m_crazy, m_canSprint, m_angerRecover, m_autoCast,
               m_group, m_change, m_crystals, m_mpScale, m_epScale, m_epPlus, m_cpScale, m_controlAngle)
};
} // namespace mugen::sim
