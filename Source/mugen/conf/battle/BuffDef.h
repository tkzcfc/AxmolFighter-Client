#pragma once

#include "mugen/conf/GameDef.h"
#include "mugen/core/reflect/Reflect.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace mugen::conf::battle
{
// 降级结果。空作用与未实现仍保留行，供报告列出。
enum class LowerStatus : int32_t
{
    Lowered = 0,     // 已降为可执行 op
    Noop = 1,        // 整行属于神器、晶体或可控特效，保留 op 但标为空作用
    Unsupported = 2, // 死规则或非法 ruleId，没有有效 op
};

// 单条 op 的生效范围。空作用仍保留原 kind，供解释器跳过。
enum class OpEffect : int32_t
{
    Active = 0,
    NoopArtifact = 1,      // 神器系统不在首批
    NoopCrystal = 2,       // 晶体消耗不在首批
    NoopControllable = 3,  // 可控特效不在首批
};

// 常驻 op 的激活 / 撤销钩子位。掩码可按位或。
enum class BuffHook : int32_t
{
    Enter = 1,  // E
    Exit = 2,   // X
    Tick = 4,   // T
    Begin = 8,  // B
    End = 16,   // N
};

// 触发点。与 BFEvent 的 1–28、50–51 同值；60 起为特殊能力专用。
enum class TriggerPoint : int32_t
{
    None = 0,
    BeforePrepareSkill = 1,
    BeforeCastSkill = 2,
    BeforeHit = 3,
    AfterHit = 4,
    BeforeToBeHit = 5,
    AfterToBeHit = 6,
    AfterCastSkill = 7,
    AfterPrepareSkill = 8,  // 已废弃，仍保留并告警
    BeforeNextSkill = 9,
    AfterNextSkill = 10,
    BeforeCrazy = 11,
    AfterCrazy = 12,
    EvadeSuccess = 13,
    BeforeDeath = 14,
    EpZero = 15,
    UseTp = 16,
    ShieldClean = 17,
    AttackMiss = 18,
    SkillColdStart = 19,
    SkillColdEnd = 20,
    HpChange = 21,
    UseEp = 22,
    BeforeArtifactSkill = 23,
    AfterArtifactSkill = 24,
    AddBuff = 25,
    RemoveBuff = 26,
    BeforeRoleSkillAction = 27,
    AfterRoleSkillAction = 28,
    BehaviorStateStart = 50,
    BehaviorStateEnd = 51,
    AbilityTime = 60,
    InitAbility = 61,
    GetUp = 62,
    HitRepel = 63,
    HitFly = 64,
    ArtifactButton = 65,
};

// 属性槽。基础属性 0–15 与运行时基础槽一致；100 起为扩展属性；200 起为倍率。
enum class BuffStat : int32_t
{
    None = -1,
    SourceForce = 0,
    Agility = 1,
    Habitus = 2,
    Spirit = 3,
    Hp = 4,
    Atk = 5,
    Def = 6,
    Matk = 7,
    Mdef = 8,
    Crit = 9,
    CritResist = 10,
    CritDamage = 11,
    CritDamageResist = 12,
    Dodge = 13,
    Hit = 14,
    BaseDamage = 15,
    AddHurt = 100,
    AvoidHurt = 101,
    AddCrit = 102,
    AvoidCrit = 103,
    AddMaxHp = 106,
    AddDodge = 109,
    AddHit = 111,
    AddCritHurt = 112,
    AddCritHurtDef = 113,
    AddArtifactHit = 114,
    AddFloorTime = 115,
    MoveScaleX = 200,
    MoveScaleZ = 201,
    ActionScale = 202,
    MpRecoveryScale = 203,
    EpRecoveryScale = 204,
    MpCostScale = 205,
    EpCostScale = 206,
    CpCostScale = 207,
    HealingScale = 208,
    ComboExpScale = 209,
    ArtifactCdRate = 210,
};

// 状态标记。1–8 与 ExtraState 同值。
enum class BuffFlag : int32_t
{
    None = 0,
    Durance = 1,
    Palsy = 2,
    Stun = 3,
    Silence = 4,
    SuperArmor = 5,
    Invincible = 6,
    Stealth = 7,
    Frozen = 8,
    NoHeal = 9,
    NoCrystalCost = 10,
    Crazy = 11,
    NotCrazy = 12,
    NotSuperArmor = 13,
};

// 资源种类。
enum class ResourceId : int32_t
{
    None = 0,
    Hp = 1,
    Mp = 2,
    Ep = 3,
    Anger = 4,
    Crystal = 5,
};

// 取值表达式种类。
enum class ValueKind : int32_t
{
    None = 0,
    Const = 1,
    PerStack = 2,
    MaxRatio = 3,
    HpAmount = 4,
    BaseMaxHp = 5,
    Lost = 6,
    Cur = 7,
    SpeedBonus = 8,
    TypeCount = 9,
    CoolingSlots = 10,
    CoolingSlotTable = 11,
    ComboTable = 12,
    LastDamage = 13,
    Reg = 14,
    CurMul = 15,      // 当前值 × p（规则 2010 激活治疗）
    HpMaxAdjust = 16, // hp/(1+p) − hp（规则 2010 的 Exit）
    RegOfMax = 17,    // 上限 × r0 × p（规则 401 的 MP）
    MaxTimes = 18,    // 上限 × TypeCount（规则 1101）
};

// 谓词种类。13 种，覆盖源条件编号。
enum class PredKind : int32_t
{
    None = 0,
    HasRule = 1,
    HasBuff = 2,
    HasBuffType = 3,
    HasFlag = 4,
    Motion = 5,
    Ratio = 6,
    Delta = 7,
    RoleType = 8,
    Skill = 9,
    Artifact = 10,
    Event = 11,
    Timer = 12,
    Misc = 13,
};

// 谓词主体。
enum class Subject : int32_t
{
    Self = 0,
    EventTarget = 1,
    Applicator = 2,
};

// 运动谓词子类。
enum class MotionCheck : int32_t
{
    None = 0,
    SpeedUp = 1,
    SpeedDown = 2,
    Air = 3,
    NotAir = 4,
};

// 资源比例的资源。
enum class RatioRes : int32_t
{
    None = 0,
    Hp = 1,
    Mp = 2,
    Ep = 3,
};

// 增减谓词子类。
enum class DeltaCheck : int32_t
{
    None = 0,
    LostHp = 1,
    RecoveredHp = 2,
    LostEp = 3,
    RecoveredEp = 4,
    HpChangePercent = 5,
};

// 角色谓词子类。
enum class RoleCheck : int32_t
{
    None = 0,
    Monster = 1,  // roleType == 4
    Hero = 2,     // roleType == 2 或 3
    TypeEq = 3,
    Sign = 4,
};

// 技能谓词子类。
enum class SkillCheck : int32_t
{
    None = 0,
    CommandAngle = 1,
    OpeningStep = 2,
    ChainFirst = 3,
    ChainLast = 4,
    ChainNotLast = 5,
    SameSlotNext = 6,
    DifferentSlot = 7,
    SlotList = 8,
    JudgeTable = 9,
    SkillId = 10,
    ActionId = 11,
    OnCooldown = 12,
};

// 神器谓词子类。首批空作用。
enum class ArtifactCheck : int32_t
{
    None = 0,
    Id = 1,
    Index = 2,
};

// 事件谓词子类。
enum class EventCheck : int32_t
{
    None = 0,
    TargetDead = 1,
    Crit = 2,
    DarkHit = 3,
    LightHit = 4,
    UseTp = 5,
    AbilityCrit = 6,
};

// 计时谓词子类。
enum class TimerCheck : int32_t
{
    None = 0,
    Moved = 1,
    Standing = 2,
    Moving = 3,  // 移动时长 > 0，分离类 Dot 的前置谓词
};

// 其余谓词子类。
enum class MiscCheck : int32_t
{
    None = 0,
    MpEnough = 1,
    CrystalEnough = 2,
    ComboEval = 3,
    RageTransform = 4,
    Never = 5,
};

// 技能修正字段。
enum class SkillModField : int32_t
{
    None = 0,
    CdScale = 1,
    MpCostScale = 2,
    EpPlus = 3,
    DisableSlot = 4,
    DisableArtifact = 5,
};

// 技能修正的作用范围。降级时按是否绑定展开。
enum class SkillScope : int32_t
{
    None = 0,
    All = 1,
    Bound = 2,
    Demand = 3,
    Slots = 4,
    Global = 5,
    BaseSlotsCrazyPlus = 6,
    ArtifactSlots = 7,
};

// 免疫选择器。
enum class ImmuneSel : int32_t
{
    None = 0,
    Rules = 1,
    BuffTypes = 2,
    All = 3,
};

// 施加目标。
enum class ApplyTo : int32_t
{
    None = 0,
    Self = 1,
    Applicator = 2,
    EventTarget = 3,
    Area = 4,
    ByChildTarget = 5,  // 未写 to 时按子 Buff 自己的 target 分派
};

// 候选选择方式。
enum class PickMode : int32_t
{
    All = 0,
    Weighted = 1,
    RandomOne = 2,
    Sequential = 3,
};

// 移除键种类。
enum class RemoveBy : int32_t
{
    None = 0,
    Id = 1,
    Rule = 2,
    Type = 3,
};

// 移除层数。
enum class StackSel : int32_t
{
    All = 0,
    Listed = 1,  // 与 id 并列的层数表
    Expr = 2,    // 层数由 m_value 求值
};

// 冷却目标。
enum class CdTarget : int32_t
{
    None = 0,
    Slots = 1,
    All = 2,
    Bound = 3,
    Demand = 4,
    ArtifactSlots = 5,
    BaseSlotsCrazyPlus = 6,
};

// 冷却操作模式。
enum class CdMode : int32_t
{
    None = 0,
    Reset = 1,
    SetRatio = 2,
    Add = 3,
    Reduce = 4,
    PerSlot = 5,
};

// 监视器动作方式。
enum class WatchMode : int32_t
{
    None = 0,
    Latched = 1,
    Edge = 2,
};

// 受击状态计时种类。
enum class StateKind : int32_t
{
    None = 0,
    GetUp = 1,
    HitRepel = 2,
    HitFly = 3,
};

// 表现开关。
enum class VisualKind : int32_t
{
    None = 0,
    SuperArmorOutline = 1,
    StealthView = 2,
    InkScreen = 3,
    WhiteFrame = 4,
};

// 计数键种类。
enum class CountBy : int32_t
{
    None = 0,
    Type = 1,
    Id = 2,
    Rule = 3,
};

// 层数比较。
enum class StackCmp : int32_t
{
    None = 0,
    AtLeast = 1,
    Equal = 2,
    NotEqual = 3,
    Below = 4,
};

// op 种类。常驻、瞬时与监视器合用一个标签。
enum class BuffOpKind : int32_t
{
    None = 0,
    Stat = 1,
    Flag = 2,
    SkillMod = 3,
    Immune = 4,
    HpLock = 5,
    Shield = 6,
    Taunt = 7,
    Kids = 8,
    Visual = 9,
    Dot = 10,
    Resource = 11,
    Apply = 12,
    Remove = 13,
    Cooldown = 14,
    Extend = 15,
    Ability = 16,
    SpawnEffect = 17,
    EnterCrazy = 18,
    Interrupt = 19,
    SetCost = 20,
    Sound = 21,
    ExitSelf = 22,
    Watch = 23,
    StateTimer = 24,
    Count = 25,
    Choice = 26,
    SummonLink = 27,
    OnSummonDeath = 28,
};

// 取值表达式。未用字段保持默认。
struct ValueExpr
{
    // 表达式种类。
    ValueKind m_kind = ValueKind::None;
    // MaxRatio / Lost / Cur / CurMul / RegOfMax / MaxTimes / HpAmount 的资源。
    ResourceId m_res = ResourceId::None;
    // TypeCount 的池：1 自身，2 事件目标（源表 p1）。
    int32_t m_pool = 0;
    // 主系数 p，或 SpeedBonus 的 a。
    Fixed m_a;
    // 次系数：下限 floor、上限 cap，或 SpeedBonus 的 b。
    Fixed m_b;
    // SpeedBonus 的上限 cap。
    Fixed m_c;
    // ComboTable 查表后再加的偏置（规则 5110 为 −1）。
    Fixed m_bias;
    // TypeCount / MaxTimes 的 buffType 列表。
    std::vector<int32_t> m_types;
    // CoolingSlotTable / ComboTable 的表。
    std::vector<Fixed> m_table;

    MG_REFLECT(ValueExpr, m_kind, m_res, m_pool, m_a, m_b, m_c, m_bias, m_types, m_table)
};

// 触发条件谓词。
struct Predicate
{
    // 谓词种类。
    PredKind m_kind = PredKind::None;
    // 求值主体。
    Subject m_subject = Subject::Self;
    // 子类（MotionCheck / DeltaCheck / RoleCheck / SkillCheck / ArtifactCheck / EventCheck / TimerCheck / MiscCheck）。
    int32_t m_subKind = 0;
    // 规则 id、Buff id 或标记。
    int32_t m_id = 0;
    // 最少层数；−1 表示只要求存在。
    int32_t m_minStacks = 0;
    // HasRule 要求 m_ids 全部存在。
    bool m_allOf = false;
    // 是否比较下界。
    bool m_hasMin = false;
    // 是否比较上界。
    bool m_hasMax = false;
    // 下界是否闭区间。
    bool m_minInclusive = true;
    // 上界是否闭区间。
    bool m_maxInclusive = true;
    // 下界或阈值。
    Fixed m_min;
    // 上界。
    Fixed m_max;
    // 多 id：规则、类型、槽位、技能、标记。
    std::vector<int32_t> m_ids;
    // 空作用标记（神器谓词、晶体谓词）。
    OpEffect m_effect = OpEffect::Active;
    // 源条件编号，0 表示不是源表条件。
    int32_t m_sourceId = 0;

    MG_REFLECT(Predicate,
               m_kind,
               m_subject,
               m_subKind,
               m_id,
               m_minStacks,
               m_allOf,
               m_hasMin,
               m_hasMax,
               m_minInclusive,
               m_maxInclusive,
               m_min,
               m_max,
               m_ids,
               m_effect,
               m_sourceId)
};

// 一条 op。按 m_kind 读取字段，其余保持默认。
// Choice 的分支放在 BuffDef::m_choicePool，不嵌套本结构。
struct BuffOp
{
    // op 种类。
    BuffOpKind m_kind = BuffOpKind::None;
    // 生效范围。
    OpEffect m_effect = OpEffect::Active;
    // 前置谓词；种类为 None 表示没有。
    Predicate m_when;
    // 主取值。
    ValueExpr m_value;
    // Stat 的属性。
    BuffStat m_stat = BuffStat::None;
    // Flag 的标记。
    BuffFlag m_flag = BuffFlag::None;
    // Stat 在每次 T 重算。
    bool m_live = false;
    // 门闩钩子上每次都重施（护盾）。
    bool m_reapply = false;
    // 只在门闩合上的那一次执行。
    bool m_onLatch = false;
    // 随机挑选一个槽或候选。
    bool m_random = false;
    // 扣血忽视护盾。
    bool m_ignoreShield = false;
    // 怒气：变身中改加冻结时间。
    bool m_transformFreeze = false;
    // 技能修正字段。
    SkillModField m_field = SkillModField::None;
    // 技能修正范围。
    SkillScope m_scope = SkillScope::None;
    // 免疫选择器。
    ImmuneSel m_immune = ImmuneSel::None;
    // 施加目标。
    ApplyTo m_to = ApplyTo::None;
    // 候选选择。
    PickMode m_pick = PickMode::All;
    // 移除键种类。
    RemoveBy m_by = RemoveBy::None;
    // 移除层数方式。
    StackSel m_stacks = StackSel::All;
    // 冷却目标。
    CdTarget m_cdTarget = CdTarget::None;
    // 冷却模式。
    CdMode m_cdMode = CdMode::None;
    // 监视方式。
    WatchMode m_watch = WatchMode::None;
    // 状态计时种类。
    StateKind m_state = StateKind::None;
    // 表现种类。
    VisualKind m_visual = VisualKind::None;
    // 计数键种类。
    CountBy m_countBy = CountBy::None;
    // 层数比较。
    StackCmp m_stackCmp = StackCmp::None;
    // 资源种类（Resource / SetCost）。
    ResourceId m_res = ResourceId::None;
    // 整数参数：层数、规则、能力 id、音效、次数、区域选择器、上限、原始状态参数。
    int32_t m_i0 = 0;
    // 第二整数参数。
    int32_t m_i1 = 0;
    // 第三整数参数。
    int32_t m_i2 = 0;
    // Choice 分支在 m_choicePool 中的起点；−1 表示没有。
    int32_t m_choiceBegin = -1;
    // 定点参数（区域 rx、冷却阈值）。
    Fixed m_f0;
    // 定点参数（区域 rz）。
    Fixed m_f1;
    // id 列表：子 Buff、槽位、规则、类型、特效。
    std::vector<int32_t> m_ids;
    // 并列整数：逐槽基准、逐 id 层数、Choice 每支长度。
    std::vector<int32_t> m_ids2;
    // 权重、逐槽阈值或 Choice 权重。
    std::vector<Fixed> m_table;
    // 逐槽数值（冷却秒数或比例）。
    std::vector<Fixed> m_extra;
    // Watch 的条件。
    std::vector<Predicate> m_predicates;

    MG_REFLECT(BuffOp,
               m_kind,
               m_effect,
               m_when,
               m_value,
               m_stat,
               m_flag,
               m_live,
               m_reapply,
               m_onLatch,
               m_random,
               m_ignoreShield,
               m_transformFreeze,
               m_field,
               m_scope,
               m_immune,
               m_to,
               m_pick,
               m_by,
               m_stacks,
               m_cdTarget,
               m_cdMode,
               m_watch,
               m_state,
               m_visual,
               m_countBy,
               m_stackCmp,
               m_res,
               m_i0,
               m_i1,
               m_i2,
               m_choiceBegin,
               m_f0,
               m_f1,
               m_ids,
               m_ids2,
               m_table,
               m_extra,
               m_predicates)
};

// 准入参数。
struct BuffAdmission
{
    // 源表规则 id。
    int32_t m_ruleId = 0;
    // 行上的 buffType（免疫类别）。
    int32_t m_buffType = 0;
    // 规则表上的 buffType（DOT 重置判断）。
    int32_t m_ruleBuffType = 0;
    // 子类型；−1 不查重。
    int32_t m_subType = 0;
    // 优先级。
    int32_t m_priority = 0;
    // 最大层数（repeatMax）。
    int32_t m_maxStacks = 0;
    // 一次去掉全部层。
    bool m_removeAllStacks = false;
    // 重复添加方式 0..7。
    int32_t m_resetType = 0;
    // 添加冷却（秒）。
    Fixed m_cd;
    // PVP 添加冷却（秒）。
    Fixed m_cdPvp;
    // 概率原值。
    Fixed m_probability;
    // 为 1 时概率乘已有层数。
    int32_t m_probabilityRepeat = 0;
    // 区域过滤。
    std::vector<int32_t> m_areaMask;
    // 目标选择器 1..10。
    int32_t m_target = 0;
    // 绑定：1 表示事件技能必须等于绑定技能。
    int32_t m_binding = 0;
    // 继承掩码。
    std::vector<int32_t> m_inherit;
    // 添加类型。
    int32_t m_addType = 0;
    // 销毁类型。
    int32_t m_destroyType = 0;
    // 方向。
    int32_t m_direction = 0;
    // 伤害类型。
    int32_t m_hurtType = 0;

    MG_REFLECT(BuffAdmission,
               m_ruleId,
               m_buffType,
               m_ruleBuffType,
               m_subType,
               m_priority,
               m_maxStacks,
               m_removeAllStacks,
               m_resetType,
               m_cd,
               m_cdPvp,
               m_probability,
               m_probabilityRepeat,
               m_areaMask,
               m_target,
               m_binding,
               m_inherit,
               m_addType,
               m_destroyType,
               m_direction,
               m_hurtType)
};

// 生命周期。语义按源表 times / interval / innerCd 原样保留。
struct BuffLifecycle
{
    // 执行类型 1 立即 / 2 等周期 / 3 等事件。
    int32_t m_executeType = 0;
    // 次数；−1 表示按间隔持续。
    int32_t m_times = 0;
    // 间隔（秒）。
    Fixed m_intervalSec;
    // 内部冷却（秒）。
    Fixed m_innerCdSec;

    MG_REFLECT(BuffLifecycle, m_executeType, m_times, m_intervalSec, m_innerCdSec)
};

// 激活方式：触发点、过滤、条件与门闩掩码。
struct BuffActivation
{
    // 开始触发点；None 表示没有 began。
    TriggerPoint m_beginOn = TriggerPoint::None;
    // 结束触发点；None 表示没有 ended。
    TriggerPoint m_endOn = TriggerPoint::None;
    // 事件目标过滤（eventParam 第一项，0 不过滤）。
    int32_t m_eventFilter = 0;
    // 触发一次后退出（eventParam 第二项为 1）。
    bool m_oneShot = false;
    // 触发时求值的条件。监视类规则把条件放在 Watch 上，这里为空。
    std::vector<Predicate> m_conditions;
    // 常驻 op 激活钩子掩码（BuffHook 按位或）。
    int32_t m_latchOn = 0;
    // 常驻 op 撤销钩子掩码。
    int32_t m_releaseOn = 0;

    MG_REFLECT(BuffActivation,
               m_beginOn,
               m_endOn,
               m_eventFilter,
               m_oneShot,
               m_conditions,
               m_latchOn,
               m_releaseOn)
};

// 表现层透传。逻辑不读这些字段。
struct BuffPresentation
{
    // Spine 资源。
    int32_t m_spineId = 0;
    // Spine 层级。
    int32_t m_spineLayer = 0;
    // Spine 偏移。
    std::vector<int32_t> m_spineOffsets;
    // Spine 步骤（霸体描边看第一项是否为 1）。
    std::vector<int32_t> m_spineStep;
    // 音效 id。
    int32_t m_audioId = 0;
    // 图标：整数或资源路径。
    std::variant<int32_t, std::string> m_icon;
    // 图标文本 id。
    int32_t m_iconDescId = 0;
    // 名称文本 id。
    int32_t m_nameId = 0;
    // 描述文本 id。
    int32_t m_descId = 0;
    // 是否弹提示。
    int32_t m_showTips = 0;

    MG_REFLECT(BuffPresentation,
               m_spineId,
               m_spineLayer,
               m_spineOffsets,
               m_spineStep,
               m_audioId,
               m_icon,
               m_iconDescId,
               m_nameId,
               m_descId,
               m_showTips)
};

// 一行 buff 的运行时定义。由 buff.lua 与 buff_rule.lua 降级。
struct BuffDef
{
    // 配置行 id。查找键为 KeyByMId。
    int32_t m_id = 0;
    // 降级状态。
    LowerStatus m_status = LowerStatus::Lowered;
    // 准入。
    BuffAdmission m_admission;
    // 生命周期。
    BuffLifecycle m_lifecycle;
    // 激活。
    BuffActivation m_activation;
    // 常驻 op：激活时施加，失活时按来源撤销。
    std::vector<BuffOp> m_persistent;
    // 进入钩子的瞬时 op。
    std::vector<BuffOp> m_onEnter;
    // 周期钩子的瞬时 op。
    std::vector<BuffOp> m_onTick;
    // 事件开始钩子的瞬时 op。
    std::vector<BuffOp> m_onBegin;
    // 事件结束钩子的瞬时 op。
    std::vector<BuffOp> m_onEnd;
    // 退出钩子的瞬时 op。
    std::vector<BuffOp> m_onExit;
    // 监视器（Watch / StateTimer）。
    std::vector<BuffOp> m_watch;
    // Choice 分支的扁平 op 池。
    std::vector<BuffOp> m_choicePool;
    // 表现透传。
    BuffPresentation m_presentation;

    MG_REFLECT(BuffDef,
               m_id,
               m_status,
               m_admission,
               m_lifecycle,
               m_activation,
               m_persistent,
               m_onEnter,
               m_onTick,
               m_onBegin,
               m_onEnd,
               m_onExit,
               m_watch,
               m_choicePool,
               m_presentation)
};

static_assert(static_cast<int32_t>(TriggerPoint::BeforePrepareSkill) ==
              static_cast<int32_t>(BFEvent::BeforePrepareSkill));
static_assert(static_cast<int32_t>(TriggerPoint::AfterRoleSkillAction) ==
              static_cast<int32_t>(BFEvent::AfterRoleSkillAction));
static_assert(static_cast<int32_t>(TriggerPoint::BehaviorStateStart) ==
              static_cast<int32_t>(BFEvent::BehaviorStateStart));
static_assert(static_cast<int32_t>(TriggerPoint::BehaviorStateEnd) == static_cast<int32_t>(BFEvent::BehaviorStateEnd));
static_assert(static_cast<int32_t>(BuffFlag::Durance) == static_cast<int32_t>(ExtraState::Durance));
static_assert(static_cast<int32_t>(BuffFlag::Frozen) == static_cast<int32_t>(ExtraState::Frozen));
static_assert(static_cast<int32_t>(BuffStat::BaseDamage) == 15);
}  // namespace mugen::conf::battle
