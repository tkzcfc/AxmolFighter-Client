#pragma once

#include "mugen/conf/battle/BuffDef.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace mugen::conf::battle
{
// 特殊能力的表现透传。
struct AbilityPresentation
{
    // Spine 资源。
    int32_t m_spineId = 0;
    // Spine 间隔。
    int32_t m_spineInterval = 0;
    // Spine 偏移。
    std::vector<int32_t> m_spineOffsets;
    // Spine 步骤。
    std::vector<int32_t> m_spineStep;
    // 音效 id。
    int32_t m_resSoundId = 0;
    // 是否弹提示。
    int32_t m_showTips = 0;
    // 图标：整数或资源路径。
    std::variant<int32_t, std::string> m_icon;
    // 名称文本 id。
    int32_t m_nameId = 0;
    // 描述文本 id。
    int32_t m_descId = 0;

    MG_REFLECT(AbilityPresentation,
               m_spineId,
               m_spineInterval,
               m_spineOffsets,
               m_spineStep,
               m_resSoundId,
               m_showTips,
               m_icon,
               m_nameId,
               m_descId)
};

// 一行特殊能力。与 Buff 共用触发点、谓词与 op。
struct AbilityDef
{
    // 配置行 id。查找键为 KeyByMId。
    int32_t m_id = 0;
    // 降级状态。整行 op 都是空作用时为 Noop。
    LowerStatus m_status = LowerStatus::Lowered;
    // 触发点（源 event 已映射到 TriggerPoint）。
    std::vector<TriggerPoint> m_events;
    // 准入谓词。空表示无条件。
    std::vector<Predicate> m_conditions;
    // 触发概率原值。
    Fixed m_probability;
    // 冷却（秒）。
    Fixed m_cd;
    // PVP 冷却（秒）。
    Fixed m_cdPvp;
    // 周期（秒）；负值表示没有周期。
    Fixed m_intervalSec;
    // 结束方式：0 不结束，1 倒计时。
    int32_t m_endedType = 0;
    // 倒计时时长（秒）。
    Fixed m_endedValue;
    // PVP 设置：0 在 PVP 中禁用，大于 1 时替换为该 id。
    int32_t m_pvpSetting = 0;
    // 优先级。
    int32_t m_priority = 0;
    // 子类型；−1 表示始终新建。
    int32_t m_subType = 0;
    // 源规则 id。
    int32_t m_ruleId = 0;
    // 绑定技能类型；非 0 时要求事件技能匹配。
    int32_t m_bindSkillType = 0;
    // 源数值参数。
    int32_t m_paramValue = 0;
    // 激活时执行的 op（生成特效、施加 Buff）。
    std::vector<BuffOp> m_ops;
    // 表现透传。
    AbilityPresentation m_presentation;

    MG_REFLECT(AbilityDef,
               m_id,
               m_status,
               m_events,
               m_conditions,
               m_probability,
               m_cd,
               m_cdPvp,
               m_intervalSec,
               m_endedType,
               m_endedValue,
               m_pvpSetting,
               m_priority,
               m_subType,
               m_ruleId,
               m_bindSkillType,
               m_paramValue,
               m_ops,
               m_presentation)
};
}  // namespace mugen::conf::battle
